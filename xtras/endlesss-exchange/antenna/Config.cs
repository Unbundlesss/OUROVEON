using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace antenna
{
    [Serializable]
    public class OutputConfig
    {
        // optional per-line font modifiers
        public string FontFace { get; set; }
        public float FontSize { get; set; }
        public bool? FontBold { get; set; }

        // offset from line above, default to 0
        public int PaddingTop { get; set; }

        // optional colour in #AABBCC form
        public string HexColour { get; set; }

        // %replacement% : %string%
        public string Formatting { get; set; }

        // optional file name to write changes into
        public string FileTarget { get; set; }
    }

    [Serializable]
    public class OutputConfigs
    {
        // global font choices
        public string FontFace { get; set; }
        public float FontSize { get; set; }
        public bool? FontBold { get; set; }

        public List<OutputConfig> Configs { get; set; }
    }

    // wrapper around a line of displayed data on the UI; handles fading in out on message change
    public class ConfigOnUI
    {
        enum DisplayState
        {
            FadeIn,
            Display,
            FadeOut,
            ChangeContents
        }

        private RGBA             DisplayRGB     = new RGBA( ConstantsFlatUI.Silver );
        private DisplayState     CurrentState   = DisplayState.ChangeContents;
        private Queue<string>    PublishQueue   = new Queue<string>();
        private float            StateTimer     = 4.0f;

        private System.Windows.Forms.Label
                                 LabelComponent;
        public string            LastEnqueued;
        public string            CurrentText { get; private set; }


        public ConfigOnUI( System.Windows.Forms.Label label, string hexColour )
        {
            LabelComponent = label;

            DisplayRGB = new RGBA( System.Drawing.ColorTranslator.FromHtml( hexColour ) );
            DisplayRGB.Alpha = 0.0;

            SyncToUI();
        }

        private void SyncToUI()
        {
            LabelComponent.Text = CurrentText;
            LabelComponent.ForeColor = DisplayRGB.ToColor();
        }

        public void Enqueue( string text )
        {
            if ( LastEnqueued != text &&
                 CurrentText != text )
            {
                LastEnqueued = text;
                PublishQueue.Enqueue( text );
            }
        }

        public void Update( float deltaTime )
        {
            StateTimer += ( deltaTime * 3.0f );

            switch ( CurrentState )
            {
                case DisplayState.FadeIn:
                {
                    DisplayRGB.Alpha = Math.Max( Math.Min( StateTimer, 1.0 ), 0.0 );

                    if ( StateTimer >= 1.0f )
                    {
                        StateTimer = 0.0f;
                        CurrentState = DisplayState.Display;
                    }
                }
                break;

                case DisplayState.Display:
                {
                    if ( PublishQueue.Count > 0 && StateTimer > 2.0f )
                    {
                        StateTimer = 0.0f;
                        CurrentState = DisplayState.FadeOut;
                    }
                }
                break;

                case DisplayState.FadeOut:
                {
                    DisplayRGB.Alpha = Math.Max( Math.Min( 1.0f - StateTimer, 1.0 ), 0.0 );

                    if ( StateTimer >= 1.0f )
                    {
                        StateTimer = 0.0f;
                        CurrentState = DisplayState.ChangeContents;
                    }
                }
                break;

                case DisplayState.ChangeContents:
                {
                    DisplayRGB.Alpha = 0.0;
                    if ( PublishQueue.Count > 0 )
                    {
                        CurrentText = PublishQueue.Dequeue();
                        StateTimer = 0.0f;
                        CurrentState = DisplayState.FadeIn;
                    }
                }
                break;
            }
            SyncToUI();
        }
    }
}
