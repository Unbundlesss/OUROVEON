using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace antenna
{
    //
    // based on ouroveon\src\r1.endlesss\endlesss\toolkit.exchange.h
    // must be exactly synchronised
    //
    [Serializable, StructLayout( LayoutKind.Sequential )]
    public struct EndlesssExchangeData
    {
        public UInt32   dataflags { get; set; }
        public UInt32   dataWriteCounter { get; }

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   _jamName;
        public string   jamName { get => _jamName; }
        public UInt64   riffHash { get; }

        public UInt64   riffTimestamp { get; }
        public UInt32   riffRoot { get; }
        public UInt32   riffScale { get; }
        public float    riffBPM { get; set; }
        public UInt32   riffBeatSegmentCount { get; }
        public UInt32   riffBeatSegmentActive { get; }

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[]  stemPulse;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[]  stemEnergy;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[]  stemGain;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt32[] stemColour;

        public float    consensusBeat;

        public float    riffTransition;

        public UInt32   jammerNameValidBits;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName1;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName2;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName3;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName4;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName5;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName6;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName7;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst =  32)]
        public string   jammerName8;

        public bool IsJammerNameValid( Int32 index )
        {
            return ( jammerNameValidBits & (1 << index) ) != 0;
        }

        public string GetJammerNameByIndex( Int32 index )
        {
            switch ( index )
            {
                case 0: return jammerName1;
                case 1: return jammerName2;
                case 2: return jammerName3;
                case 3: return jammerName4;
                case 4: return jammerName5;
                case 5: return jammerName6;
                case 6: return jammerName7;
                case 7: return jammerName8;
            }
            return "";
        }
    }

    public static class Constants
    {
        public static List<string> cScaleNames = new List<string>
        {
            "Major (Ionian)",
            "Dorian",
            "Phrygian",
            "Lydian",
            "Mixolydian",
            "Minor (Aeolian)",
            "Locrian",
            "Minor Pentatonic",
            "Major Pentatonic",
            "Suspended Pent.",
            "Blues Minor Pent.",
            "Blues Major Pent.",
            "Harmonic Minor",
            "Melodic Minor",
            "Double Harmonic",
            "Blues",
            "Whole Tone",
            "Chromatic"
        };
        // ---------------------------------------------------------------------------------------------------------------------
        public static List<string> cRootNames = new List<string>
        {
            "C",
            "Db",
            "D",
            "Eb",
            "E",
            "F",
            "F#",
            "G",
            "Ab",
            "A",
            "Bb",
            "B"
        };
    }
}
