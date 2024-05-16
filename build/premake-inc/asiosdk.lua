
-- ==============================================================================
-- we need the Steinberg ASIO SDK on Windows for PortAudio, but Steinberg say we 
-- can't check it in, so .. go download it from a stable URL and unpack it into place

function asioDownloadProgress(total, current)
  local ratio = current / total;
  ratio = math.min(math.max(ratio, 0), 1);
  local percent = math.floor(ratio * 100);
  print("ASIO SDK download progress (" .. percent .. "%/100%)")
end

function asioSDKVersion()
    return "asiosdk_2.3.3_2019-06-14"
end

function checkASIOSDKDownload( buildRootDir )
    local asioSdkFile   = asioSDKVersion() .. ".zip"
    local asioSdkFolder = "asiosdk"

    if ( GeneratingForVisualStudio() ) then

        local asioSdkFileLocalPath, err = os.realpath( path.join( buildRootDir, asioSdkFile ) )

        print("Checking ASIO SDK : " .. asioSdkFileLocalPath)
        if ( os.isfile(asioSdkFileLocalPath) == false ) then

            local result_str, response_code = http.download(
                "https://web.archive.org/web/20231011192709/https://download.steinberg.net/sdk_downloads/" .. asioSdkFile,
                asioSdkFileLocalPath, {
                    progress = asioDownloadProgress,
                    })

        end

        local asioTargetDir = path.join( SrcDir(), "r0.hal", asioSdkFolder)
        local asioRealPath, err = os.realpath(asioTargetDir)

        if ( asioRealPath == nil ) then
            print("Failed to resolve ASIO path : " .. asioTargetDir .. " : " .. err)
        else
            print("Checking ASIO deployment : " .. asioRealPath)
            if ( os.isdir(asioTargetDir) == false ) then
                zip.extract(asioSdkFileLocalPath, asioRealPath)

                local asioListFileToPatch = os.realpath( path.join( asioRealPath, asioSDKVersion(), "host", "pc", "asiolist.cpp") )

                local asioListContents = io.readfile(asioListFileToPatch)

                -- dumb patching of Win32 calls to force MBCS versions as this file can't compile under Unicode
                asioListContents = string.gsub(asioListContents, 'CharLowerBuff%(',      '::CharLowerBuffA(')
                asioListContents = string.gsub(asioListContents, 'RegOpenKey%(',         '::RegOpenKeyA(' )
                asioListContents = string.gsub(asioListContents, 'RegEnumKey%(',         '::RegEnumKeyA(' )
                asioListContents = string.gsub(asioListContents, 'RegQueryValueEx%(',    '::RegQueryValueExA(' )
                asioListContents = string.gsub(asioListContents, ',%(LPTSTR%)',          ',(LPSTR)' )
                asioListContents = string.gsub(asioListContents, ',strlen',              ', (DWORD)strlen' )

                local ok, err = os.rename( asioListFileToPatch, asioListFileToPatch .. "_old" )
                if not ok then
                    error(err)
                end
                io.writefile(asioListFileToPatch, asioListContents)
            end
        end

    end
end
