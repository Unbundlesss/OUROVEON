using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.CompilerServices;

namespace antenna
{
    // https://www.materialui.co/flatuicolors
    public static class ConstantsFlatUI
    {
        public static readonly Color Turqoise           = Color.FromArgb( 26,188,156 );
        public static readonly Color Emerland           = Color.FromArgb( 46,204,113 );
        public static readonly Color PeterRiver         = Color.FromArgb( 52,152,219 );
        public static readonly Color Amethyst           = Color.FromArgb( 155,89,182 );
        public static readonly Color WetAsphalt         = Color.FromArgb( 52,73,94 );
                                                                          
        public static readonly Color GreenSea           = Color.FromArgb( 22,160,133 );
        public static readonly Color Nephritis          = Color.FromArgb( 39,174,96 );
        public static readonly Color BelizeHole         = Color.FromArgb( 41,128,185 );
        public static readonly Color Wisteria           = Color.FromArgb( 142,68,173 );
        public static readonly Color MidnightBlue       = Color.FromArgb( 44,62,80 );
                                                                          
        public static readonly Color Sunflower          = Color.FromArgb( 241,196,15 );
        public static readonly Color Carrot             = Color.FromArgb( 230,126,34 );
        public static readonly Color Alizarin           = Color.FromArgb( 231,76,60 );
        public static readonly Color Clouds             = Color.FromArgb( 236,240,241 );
        public static readonly Color Concrete           = Color.FromArgb( 149,165,166 );
                                                                          
        public static readonly Color Orange             = Color.FromArgb( 243,156,18 );
        public static readonly Color Pumpkin            = Color.FromArgb( 211,84,0 );
        public static readonly Color Pomegranate        = Color.FromArgb( 192,57,43 );
        public static readonly Color Silver             = Color.FromArgb( 189,195,199 );
        public static readonly Color Asbestos           = Color.FromArgb( 127,140,141 );
    }

    public class RGBA 
    {
        public double   Red        = 0;
        public double   Green      = 0;
        public double   Blue       = 0;
        public double   Alpha      = 0;

        public override string ToString() => "RGBA";

        public RGBA()
        {
        }

        public RGBA( in double inR, in double inG, in double inB, in double inA )
        {
            Red     = inR;
            Green   = inG;
            Blue    = inB;
            Alpha   = inA;
        }

        public RGBA( in Color from )
        {
            Red     = (double)from.R / 255.0;
            Green   = (double)from.G / 255.0;
            Blue    = (double)from.B / 255.0;
            Alpha   = 1.0;
        }

        public Color ToColor()
        {
            return Color.FromArgb(
                (int)( Alpha * 255.0),
                (int)( Red * Alpha * 255.0 ),
                (int)( Green * Alpha * 255.0),
                (int)( Blue * Alpha * 255.0) );
        }
    }
}
