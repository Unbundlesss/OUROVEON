using System;
using System.IO;
using System.Windows.Forms;

using Newtonsoft.Json;

namespace antenna
{
    static class Program
    {
        private static readonly string configFileName = "config.json";

        [STAThread]
        static void Main()
        {
            if ( !File.Exists( configFileName ) )
            {
                MessageBox.Show( $"Cannot find configuration file [{configFileName}]", "Antenna Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
                return;
            }

            string configData = File.ReadAllText( configFileName );
            if ( string.IsNullOrEmpty( configData ) )
            {
                MessageBox.Show( $"Config file [{configFileName}] empty", "Antenna Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
                return;
            }

            OutputConfigs cfg;
            try
            {
                cfg = JsonConvert.DeserializeObject<OutputConfigs>( configData );
            }
            catch ( Exception ex )
            {
                MessageBox.Show( $"Config file deserialize failed, {ex.Message}", "Antenna Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
                return;
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault( false );
            Application.Run( new FormAntenna(cfg) );
        }
    }
}
