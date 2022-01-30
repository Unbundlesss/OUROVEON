
namespace antenna
{
    partial class FormAntenna
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose( bool disposing )
        {
            if ( disposing && (components != null) )
            {
                components.Dispose();
            }
            base.Dispose( disposing );
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(FormAntenna));
            this.hwFlow = new System.Windows.Forms.FlowLayoutPanel();
            this.hwUpdateTimer = new System.Windows.Forms.Timer(this.components);
            this.SuspendLayout();
            // 
            // hwFlow
            // 
            this.hwFlow.Dock = System.Windows.Forms.DockStyle.Fill;
            this.hwFlow.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
            this.hwFlow.Location = new System.Drawing.Point(0, 0);
            this.hwFlow.Name = "hwFlow";
            this.hwFlow.Padding = new System.Windows.Forms.Padding(16);
            this.hwFlow.Size = new System.Drawing.Size(684, 361);
            this.hwFlow.TabIndex = 0;
            // 
            // hwUpdateTimer
            // 
            this.hwUpdateTimer.Enabled = true;
            this.hwUpdateTimer.Interval = 20;
            this.hwUpdateTimer.Tick += new System.EventHandler(this.hwUpdateTimer_Tick);
            // 
            // FormAntenna
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.Black;
            this.ClientSize = new System.Drawing.Size(684, 361);
            this.Controls.Add(this.hwFlow);
            this.ForeColor = System.Drawing.Color.White;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "FormAntenna";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Antenna Display";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.FormAntenna_FormClosing);
            this.Load += new System.EventHandler(this.FormAntenna_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.FlowLayoutPanel hwFlow;
        private System.Windows.Forms.Timer hwUpdateTimer;
    }
}

