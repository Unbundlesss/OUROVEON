//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "endlesss/core.types.h"
#include "endlesss/api.h"

namespace endlesss {
namespace types {

// ---------------------------------------------------------------------------------------------------------------------
Stem::Stem( const JamCouchID& jamCouchID, const endlesss::api::ResultStemDocument& stemFromNetwork )
    : couchID( stemFromNetwork._id )
    , jamCouchID( jamCouchID )
    , fileEndpoint( stemFromNetwork.cdn_attachments.oggAudio.endpoint )
    , fileBucket( stemFromNetwork.cdn_attachments.oggAudio.bucket )
    , fileKey( stemFromNetwork.cdn_attachments.oggAudio.key )
    , fileMIME( stemFromNetwork.cdn_attachments.oggAudio.mime )
    , fileLengthBytes( stemFromNetwork.cdn_attachments.oggAudio.length )
    , sampleRate( (int32_t)stemFromNetwork.sampleRate )
    , creationTimeUnix( stemFromNetwork.created / 1000 ) // from unix nano
    , preset( stemFromNetwork.presetName )
    , user( stemFromNetwork.creatorUserName )
    , colour( stemFromNetwork.primaryColour )
    , BPS( stemFromNetwork.bps )
    , BPMrnd( BPStoRoundedBPM( stemFromNetwork.bps ) )
    , length16s( stemFromNetwork.length16ths )
    , originalPitch( stemFromNetwork.originalPitch )
    , barLength( stemFromNetwork.barLength )
    , isDrum( stemFromNetwork.isDrum )
    , isNote( stemFromNetwork.isNote )
    , isBass( stemFromNetwork.isBass )
    , isMic( stemFromNetwork.isMic )
{
}

// ---------------------------------------------------------------------------------------------------------------------
Riff::Riff( const JamCouchID& jamCouchID, const endlesss::api::ResultRiffDocument& fromNetwork )
    : couchID( fromNetwork._id )
    , jamCouchID( jamCouchID )
    , user( fromNetwork.userName )
    , creationTimeUnix( fromNetwork.created / 1000 ) // from unix nano
    , root( fromNetwork.root )
    , scale( fromNetwork.scale )
    , BPS( fromNetwork.state.bps )
    , BPMrnd( BPStoRoundedBPM( fromNetwork.state.bps ) )
    , barLength( fromNetwork.state.barLength )
    , appVersion( fromNetwork.app_version )
    , magnitude( fromNetwork.magnitude )
{
    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        stemsOn[stemI] = fromNetwork.state.playback[stemI].slot.current.on;
        stems[stemI]   = StemCouchID{ stemsOn[stemI] ? fromNetwork.state.playback[stemI].slot.current.currentLoop : "" };
        gains[stemI]   = fromNetwork.state.playback[stemI].slot.current.gain;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
RiffComplete::RiffComplete( const endlesss::api::pull::LatestRiffInJam& riffDetails )
    : jam( riffDetails.m_jamCouchID, riffDetails.m_jamDisplayName )
    , riff( riffDetails.m_jamCouchID, riffDetails.getRiffDetails().rows[0].doc )
{
    const auto& allStemRows = riffDetails.getStemDetails().rows;

    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        if ( riff.stemsOn[stemI] )
        {
            bool foundMatchingStem = false;
            for ( const auto& stemRow : allStemRows )
            {
                if ( stemRow.id == riff.stems[stemI] )
                {
                    stems[stemI] = { riffDetails.m_jamCouchID, stemRow.doc };
                    foundMatchingStem = true;
                    break;
                }
            }
            assert( foundMatchingStem );
        }
        else
        {
            stems[stemI] = {};
        }
    }
}

} // namespace types
} // namespace endlesss
