using System;
using System.Collections.Generic;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Win32.SafeHandles;

namespace antenna
{
    public static class Native
    {
        public const UInt32 STANDARD_RIGHTS_REQUIRED = 0x000F0000;
        public const UInt32 SECTION_QUERY            = 0x0001;
        public const UInt32 SECTION_MAP_WRITE        = 0x0002;
        public const UInt32 SECTION_MAP_READ         = 0x0004;
        public const UInt32 SECTION_MAP_EXECUTE      = 0x0008;
        public const UInt32 SECTION_EXTEND_SIZE      = 0x0010;
        public const UInt32 SECTION_ALL_ACCESS       = (STANDARD_RIGHTS_REQUIRED |
                                                        SECTION_QUERY |
                                                        SECTION_MAP_WRITE |
                                                        SECTION_MAP_READ |
                                                        SECTION_MAP_EXECUTE |
                                                        SECTION_EXTEND_SIZE);
        public const UInt32 FILE_MAP_ALL_ACCESS      = SECTION_ALL_ACCESS;


        [DllImport( "kernel32.dll", SetLastError = true, CharSet = CharSet.Auto )]
        static public extern SafeFileHandle OpenFileMapping(
              uint dwDesiredAccess,
              bool bInheritHandle,
              string lpName );

        [DllImport( "kernel32.dll", SetLastError = true )]
        static public extern IntPtr MapViewOfFile(
            SafeFileHandle hFileMappingObject,
            UInt32 dwDesiredAccess,
            UInt32 dwFileOffsetHigh,
            UInt32 dwFileOffsetLow,
            UIntPtr dwNumberOfBytesToMap );
    }

    class Antenna
    {
        static private readonly SecurityIdentifier securityWorldSID = new SecurityIdentifier( WellKnownSidType.WorldSid , null );

        // matching identifiers in the GlobalTransmission class in ouroveon\src\r1.endlesss\endlesss\toolkit.exchange.h
        private static readonly string cGlobalMapppingName       = "Ouroveon_EXCH";
        private static readonly string cGlobalMutexName          = "Global\\Mutex_Ouroveon_EXCH";
        private static readonly UInt32  cSharedBufferSize        = (UInt32)Marshal.SizeOf(typeof(EndlesssExchangeData));

        private static SafeFileHandle sHandle;
        private static IntPtr hHandle = IntPtr.Zero;
        private static IntPtr pBuffer = IntPtr.Zero;

        public static bool Bind()
        {
            if ( sHandle != null && !sHandle.IsInvalid )
                return false;

            sHandle = Native.OpenFileMapping( Native.FILE_MAP_ALL_ACCESS, false, cGlobalMapppingName );
            if ( sHandle.IsInvalid )
                return false;

            pBuffer = Native.MapViewOfFile( sHandle, Native.FILE_MAP_ALL_ACCESS, 0, 0, new UIntPtr( cSharedBufferSize ) );

            return true;
        }

        public static bool Unbind()
        {
            if ( !sHandle.IsInvalid &&
                 !sHandle.IsClosed )
            {
                sHandle.Close();
            }
            pBuffer = IntPtr.Zero;
            hHandle = IntPtr.Zero;

            return true;
        }

        public static bool IsBound()
        {
            return sHandle != null && 
                  !sHandle.IsInvalid && 
                  !sHandle.IsClosed && 
                   pBuffer != IntPtr.Zero;
        }

        public static bool Read( out EndlesssExchangeData data )
        {
            if ( sHandle.IsInvalid )
            {
                data = new EndlesssExchangeData { dataflags = 0 };
                return false;
            }

            data = Marshal.PtrToStructure<EndlesssExchangeData>( pBuffer );
            return true;
        }

    }
}
