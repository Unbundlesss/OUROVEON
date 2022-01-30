using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;

namespace antenna
{
    public partial class FormAntenna : Form
    {
        // the config data from disk and UI proxies
        private readonly OutputConfigs configs;
        private readonly Dictionary<OutputConfig, ConfigOnUI> ConfigsAndUI = new Dictionary<OutputConfig, ConfigOnUI>();

        // a prepared list of %metadata_name% -> reflected property, used when processing strings eg "%jamName%:%timeStamp%"
        private readonly Dictionary<string, PropertyInfo> MetadataReplacements = new Dictionary<string, PropertyInfo>();

        // extra %symbols% for data computed outside of the exchange data - ie a unique set of jammer names
        private readonly Dictionary<string, string> DynamicReplacements = new Dictionary<string, string>();


        private HashSet<string> UniqueJammers = new HashSet<string>();
        private HashSet<string> UniqueJammersUpper = new HashSet<string>();


        public FormAntenna( OutputConfigs cfg )
        {
            configs = cfg;

            PropertyInfo[] properties = typeof(EndlesssExchangeData).GetProperties();
            foreach ( PropertyInfo property in properties )
            {
                MetadataReplacements.Add( $"%{property.Name}%", property );
            }

            InitializeComponent();

            Int32 controlID = 1;
            foreach ( var c in configs.Configs )
            {
                var outputLabel = new System.Windows.Forms.Label();

                outputLabel.AutoSize = true;
                outputLabel.Font = new System.Drawing.Font( configs.FontFace, configs.FontSize, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)) );
                outputLabel.ForeColor = System.Drawing.Color.Gainsboro;
                outputLabel.Location = new System.Drawing.Point( 3, 0 );
                outputLabel.Name = $"hwLabel{controlID}";
                outputLabel.TabIndex = controlID;
                outputLabel.Text = "";
                hwFlow.Controls.Add( outputLabel );

                var defaultColour = "#EEEEEE";
                if ( !string.IsNullOrEmpty( c.HexColour ) )
                    defaultColour = c.HexColour;

                var outputConfig = new ConfigOnUI( outputLabel, defaultColour );

                ConfigsAndUI.Add( c, outputConfig );

                controlID++;
            }
        }

        private bool runReadThread;
        private void AntennaReadThread()
        {
            UInt32 lastWriteCounter = 0;
            while ( runReadThread )
            {
                if ( Antenna.Read(out var data) )
                {
                    BeginInvoke( new Action( () =>
                    {
                        bool shouldSend = ( data.dataWriteCounter != lastWriteCounter );
                        lastWriteCounter = data.dataWriteCounter;
                        if ( shouldSend )
                        {
                            NewDataArrived( data );
                        }
                    } ) );
                }
                Thread.Sleep( 250 );
            }
        }

        public static DateTime UnixTimeStampToDateTime( UInt64 unixTimeStamp )
        {
            // Unix timestamp is seconds past epoch
            DateTime dateTime = new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc);
            dateTime = dateTime.AddSeconds( unixTimeStamp ).ToLocalTime();
            return dateTime;
        }

        private void Execute( EndlesssExchangeData metadata )
        {
            // compute some string reps from data
            DateTime riffTime = UnixTimeStampToDateTime( metadata.riffTimestamp );
            DynamicReplacements["%time_short%"] = riffTime.ToString( "yyyy-MM-dd" );
            DynamicReplacements["%time_long%"] = riffTime.ToString( "F" );
            DynamicReplacements["%scale%"] = Constants.cScaleNames[(int)metadata.riffScale];
            DynamicReplacements["%root%"] = Constants.cRootNames[(int)metadata.riffRoot];

            // compute a "unique jammers" string, "foo x bar x who" style
            UniqueJammers.Clear();
            UniqueJammersUpper.Clear();
            for ( int stem = 0; stem < 8; stem++ )
            {
                if ( !metadata.IsJammerNameValid( stem ) )
                    continue;

                var jammerName = metadata.GetJammerNameByIndex( stem );
                {
                    UniqueJammers.Add( jammerName );
                    UniqueJammersUpper.Add( jammerName.ToUpper() );
                }
            }
            {
                String[] jammerList = new String[UniqueJammers.Count];
                UniqueJammers.CopyTo( jammerList );
                Array.Sort( jammerList );

                var newJamHeadline = string.Join(" x ", jammerList);
                DynamicReplacements[ "%unique_jammers%" ] = newJamHeadline;
            }
            {
                String[] jammerList = new String[UniqueJammersUpper.Count];
                UniqueJammersUpper.CopyTo( jammerList );
                Array.Sort( jammerList );

                var newJamHeadline = string.Join(" x ", jammerList);
                DynamicReplacements["%unique_jammers_upper%"] = newJamHeadline;
            }


            // update all the config definitions using the replacements; write to disk and update UI
            foreach ( var cui in ConfigsAndUI )
            {
                string result = cui.Key.Formatting.Clone() as string;

                foreach ( var replacement in MetadataReplacements )
                {
                    result = result.Replace( replacement.Key, replacement.Value.GetValue( metadata ).ToString() );
                }
                foreach ( var replacement in DynamicReplacements )
                {
                    result = result.Replace( replacement.Key, replacement.Value );
                }

                System.IO.File.WriteAllText( cui.Key.FileTarget, result );
                cui.Value.Enqueue( result );
            }
        }

        private void NewDataArrived( EndlesssExchangeData data )
        {
            Execute( data );
        }

        private void FormAntenna_Load( object sender, EventArgs e )
        {
            Antenna.Bind();

            runReadThread = Antenna.IsBound();
            new Thread( () => AntennaReadThread() ) { IsBackground = true }.Start();
        }

        private void FormAntenna_FormClosing( object sender, FormClosingEventArgs e )
        {
            Antenna.Unbind();
        }

        private void hwUpdateTimer_Tick( object sender, EventArgs e )
        {
            foreach ( var cui in ConfigsAndUI )
            {
                cui.Value.Update( 0.02f );
            }
            Refresh();
        }
    }
}
