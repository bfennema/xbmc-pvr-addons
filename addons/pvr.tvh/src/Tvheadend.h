#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "client.h"
#include "platform/sockets/tcp.h"
#include "platform/threads/threads.h"
#include "platform/threads/mutex.h"
#include "platform/util/buffer.h"
#include "xbmc_codec_types.h"
#include "xbmc_stream_utils.hpp"
#include "libXBMC_addon.h"
#include "CircBuffer.h"
#include "HTSPTypes.h"
#include "AsyncState.h"
#include <map>
#include <queue>
#include <cstdarg>
#include <stdexcept>

extern "C" {
#include <sys/types.h>
#include "libhts/htsmsg.h"
}

/*
 * Miscellaneous
 */
#if defined(__GNUC__)
#define _unused(x) x __attribute__((unused))
#else
#define _unused(x) x
#endif

/*
 * Configuration defines
 */
#define HTSP_API_VERSION           (13)
#define FAST_RECONNECT_ATTEMPTS     (5)
#define FAST_RECONNECT_INTERVAL   (500) // ms
#define UNNUMBERED_CHANNEL      (10000)
#define INVALID_SEEKTIME           (-1)

/*
 * Recoding types defines
 */
#define REC_ONCE                   1
#define REC_EVERYTIME              2
#define REC_EVERY_WEEK_THIS_TIME   3
#define REC_EVERY_DAY_THIS_TIME    4
#define REC_WEEKENDS               5
#define REC_WEEKDAYS               6

/*
 * Log wrappers
 */
#define tvhdebug(...) tvhlog(LOG_DEBUG, ##__VA_ARGS__)
#define tvhinfo(...)  tvhlog(LOG_INFO,  ##__VA_ARGS__)
#define tvherror(...) tvhlog(LOG_ERROR, ##__VA_ARGS__)
#define tvhtrace(...) if (g_bTraceDebug) tvhlog(LOG_DEBUG, ##__VA_ARGS__)
static inline void tvhlog ( ADDON::addon_log_t lvl, const char *fmt, ... )
{
  char buf[16384];
  size_t c = sprintf(buf, "pvr.tvh - ");
  va_list va;
  va_start(va, fmt);
  vsnprintf(buf + c, sizeof(buf) - c, fmt, va);
  va_end(va);
  XBMC->Log(lvl, "%s", buf);
}

/*
 * Forward decleration of classes
 */
class CTvheadend;
class CHTSPConnection;
class CHTSPDemuxer;
class CHTSPVFS;
class CHTSPResponse;
class CHTSPMessage;

/* Typedefs */
typedef std::runtime_error AuthException;
typedef std::map<uint32_t,CHTSPResponse*> CHTSPResponseList;
typedef PLATFORM::SyncedBuffer<CHTSPMessage> CHTSPMessageQueue;

/*
 * Global (TODO: might want to change this)
 */
extern CTvheadend *tvh;

/*
 * HTSP Response handler
 */
class CHTSPResponse
{
public:
  CHTSPResponse(void);
  ~CHTSPResponse();
  htsmsg_t *Get ( PLATFORM::CMutex &mutex, uint32_t timeout );
  void      Set ( htsmsg_t *m );
private:
  PLATFORM::CCondition<volatile bool> m_cond;
  bool                                m_flag;
  htsmsg_t                           *m_msg;
};

/*
 * HTSP Message
 */
class CHTSPMessage
{
public:
  CHTSPMessage(std::string method = "", htsmsg_t *msg = NULL)
    : m_method(method), m_msg(msg)
  {
  }
  CHTSPMessage(const CHTSPMessage& msg)
    : m_method(msg.m_method), m_msg(msg.m_msg)
  {
    msg.m_msg    = NULL;
  }
  ~CHTSPMessage()
  {
    if (m_msg)
      htsmsg_destroy(m_msg);
  }
  CHTSPMessage& operator=(const CHTSPMessage &msg)
  {
    if (this != &msg)
    {
      if (m_msg)
        htsmsg_destroy(m_msg);
      m_method     = msg.m_method;
      m_msg        = msg.m_msg;
      msg.m_msg    = NULL; // ownership is passed
    }
    return *this;
  }
  std::string      m_method;
  mutable htsmsg_t *m_msg;
};

/*
 * HTSP Connection registration thread
 */
class CHTSPRegister
  : public PLATFORM::CThread
{
  friend class CHTSPConnection;

public:
  CHTSPRegister ( CHTSPConnection *conn );
  ~CHTSPRegister ( void );
 
private:
  CHTSPConnection *m_conn;
  void *Process ( void );
};

/*
 * HTSP Connection
 */
class CHTSPConnection
  : public PLATFORM::CThread
{
  friend class CHTSPRegister;

public:
  CHTSPConnection();
  ~CHTSPConnection();

  void Disconnect  ( void );
  
  bool      SendMessage0    ( const char *method, htsmsg_t *m );
  htsmsg_t *SendAndWait0    ( const char *method, htsmsg_t *m, int iResponseTimeout = -1);
  htsmsg_t *SendAndWait     ( const char *method, htsmsg_t *m, int iResponseTimeout = -1 );

  inline int  GetProtocol      ( void ) const { return m_htspVersion; }

  CStdString  GetWebURL        ( const char *fmt, ... );

  const char *GetServerName    ( void );
  const char *GetServerVersion ( void );
  const char *GetServerString  ( void );
  
  bool        HasCapability(const std::string &capability) const;

  inline bool IsConnected       ( void ) const { return m_ready; }
  bool        WaitForConnection ( void );

  inline PLATFORM::CMutex& Mutex ( void ) { return m_mutex; }
  
private:
  void*       Process          ( void );
  void        Register         ( void );
  bool        ReadMessage      ( void );
  bool        SendHello        ( void );
  void        SendAuth         ( const CStdString &u, const CStdString &p );

  PLATFORM::CTcpSocket               *m_socket;
  PLATFORM::CMutex                    m_mutex;
  CHTSPRegister                       m_regThread;
  PLATFORM::CCondition<volatile bool> m_regCond;
  bool                                m_ready;
  uint32_t                            m_seq;
  CStdString                          m_serverName;
  CStdString                          m_serverVersion;
  int                                 m_htspVersion;
  CStdString                          m_webRoot;
  void*                               m_challenge;
  int                                 m_challengeLen;

  CHTSPResponseList                   m_messages;
  std::vector<std::string>            m_capabilities;
};

/*
 * HTSP Demuxer - live streams
 */
class CHTSPDemuxer
{
  friend class CTvheadend;

public:
  CHTSPDemuxer( CHTSPConnection &conn );
  ~CHTSPDemuxer();

  bool   ProcessMessage ( const char *method, htsmsg_t *m );
  void   Connected      ( void );
  
  inline time_t GetTimeshiftTime() const
  {
    return (time_t)m_timeshiftStatus.shift;
  }

private:
  PLATFORM::CMutex                        m_mutex;
  CHTSPConnection                        &m_conn;
  PLATFORM::SyncedBuffer<DemuxPacket*>    m_pktBuffer;
  ADDON::XbmcStreamProperties             m_streams;
  std::map<int,int>                       m_streamStat;
  int64_t                                 m_seekTime;
  PLATFORM::CCondition<volatile int64_t>  m_seekCond;
  SSourceInfo                             m_sourceInfo;
  SQuality                                m_signalInfo;
  STimeshiftStatus                        m_timeshiftStatus;
  SSubscription                           m_subscription;
  
  void         Close0         ( void );
  void         Abort0         ( void );
  bool         Open           ( const PVR_CHANNEL &chn );
  void         Close          ( void );
  DemuxPacket *Read           ( void );
  void         Flush          ( void );
  void         Abort          ( void );
  bool         Seek           ( int time, bool backwards, double *startpts );
  void         Speed          ( int speed );
  int          CurrentId      ( void );
  PVR_ERROR    CurrentStreams ( PVR_STREAM_PROPERTIES *streams );
  PVR_ERROR    CurrentSignal  ( PVR_SIGNAL_STATUS &sig );

  void SendSubscribe   ( bool force = false );
  void SendUnsubscribe ( void );
  void SendSpeed       ( bool force = false );
  
  void ParseMuxPacket           ( htsmsg_t *m );
  void ParseSourceInfo          ( htsmsg_t *m );
  void ParseSubscriptionStart   ( htsmsg_t *m );
  void ParseSubscriptionStop    ( htsmsg_t *m );
  void ParseSubscriptionSkip    ( htsmsg_t *m );
  void ParseSubscriptionSpeed   ( htsmsg_t *m );
  void ParseSubscriptionStatus  ( htsmsg_t *m );
  void ParseQueueStatus         ( htsmsg_t *m );
  void ParseSignalStatus        ( htsmsg_t *m );
  void ParseTimeshiftStatus     ( htsmsg_t *m );
};

/*
 * HTSP VFS - recordings
 */
class CHTSPVFS
{
  friend class CTvheadend;

public:
  CHTSPVFS ( CHTSPConnection &conn );
  ~CHTSPVFS ();

  void Connected    ( void );

private:
  CHTSPConnection &m_conn;
  CStdString      m_path;
  uint32_t        m_fileId;
  CCircBuffer     m_buffer;
  int64_t         m_offset;

  bool      Open   ( const PVR_RECORDING &rec );
  void      Close  ( void );
  int       Read   ( unsigned char *buf, unsigned int len );
  long long Seek   ( long long pos, int whence );
  long long Tell   ( void );
  long long Size   ( void );

  bool      SendFileOpen  ( bool force = false );
  void      SendFileClose ( void );
  ssize_t   SendFileSeek  ( int64_t pos, int whence, bool force = false );
};

/*
 * Root object for Tvheadend connection
 */
class CTvheadend
  : public PLATFORM::CThread
{
public:
  CTvheadend();
  ~CTvheadend();

  void Start ( void );

  void Disconnected   ( void );
  bool Connected      ( void );
  bool ProcessMessage ( const char *method, htsmsg_t *msg );

  PVR_ERROR GetDriveSpace     ( long long *total, long long *used );

  int       GetTagCount       ( void );
  PVR_ERROR GetTags           ( ADDON_HANDLE handle );
  PVR_ERROR GetTagMembers     ( ADDON_HANDLE handle,
                                const PVR_CHANNEL_GROUP &group );

  int       GetChannelCount   ( void );
  PVR_ERROR GetChannels       ( ADDON_HANDLE handle, bool radio );

  int       GetRecordingCount ( void );
  PVR_ERROR GetRecordings     ( ADDON_HANDLE handle );
  PVR_ERROR GetRecordingEdl   ( const PVR_RECORDING &rec, PVR_EDL_ENTRY edl[],
                                int *num );
  PVR_ERROR DeleteRecording   ( const PVR_RECORDING &rec );
  PVR_ERROR RenameRecording   ( const PVR_RECORDING &rec );
  int       GetTimerCount     ( void );
  PVR_ERROR GetTimers         ( ADDON_HANDLE handle );
  PVR_ERROR AddTimer          ( const PVR_TIMER &tmr );
  PVR_ERROR DeleteTimer       ( const PVR_TIMER &tmr, bool force );
  PVR_ERROR UpdateTimer       ( const PVR_TIMER &tmr );

  PVR_ERROR GetEpg            ( ADDON_HANDLE handle, const PVR_CHANNEL &chn,
                                time_t start, time_t end );
  
private:
  uint32_t GetNextUnnumberedChannelNumber();
  
  PLATFORM::CMutex            m_mutex;
  
  CHTSPConnection             m_conn;
  CHTSPDemuxer                m_dmx;
  CHTSPVFS                    m_vfs;

  CHTSPMessageQueue           m_queue;

  SChannels                   m_channels;
  STags                       m_tags;
  SRecordings                 m_recordings;
  SSchedules                  m_schedules;

  SHTSPEventList              m_events;

  AsyncState                  m_asyncState;
  
  CStdString  GetImageURL     ( const char *str );

  /*
   * Message processing
   */
  void *Process ( void );

  /*
   * Event handling
   */
  inline void TriggerChannelGroupsUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_TAG_UPDATE));
  }
  inline void TriggerChannelUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_CHN_UPDATE));
  }
  inline void TriggerRecordingUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
  }
  inline void TriggerTimerUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
  }
  inline void TriggerEpgUpdate ( uint32_t idx )
  {
    SHTSPEvent event = SHTSPEvent(HTSP_EVENT_EPG_UPDATE, idx);
    
    if (std::find(m_events.begin(), m_events.end(), event) == m_events.end())
      m_events.push_back(event);
  }

  /*
   * Epg Handling
   */
  void        TransferEvent   ( ADDON_HANDLE handle, const SEvent &event );

  /*
   * Message sending
   */
  PVR_ERROR   SendDvrAutorecDelete (CStdString &id);
  PVR_ERROR   SendDvrDelete        ( uint32_t id, const char *method );
  PVR_ERROR   SendDvrUpdate        ( uint32_t id, const CStdString &title,
                                     time_t start, time_t stop );

  /*
   * Channel/Tags/Recordings/Events
   */
  void SyncChannelsCompleted ( void );
  void SyncDvrCompleted      ( void );
  void SyncEpgCompleted      ( void );
  void SyncCompleted         ( void );
  void ParseTagUpdate       ( htsmsg_t *m );
  void ParseTagDelete       ( htsmsg_t *m );
  void ParseChannelUpdate   ( htsmsg_t *m );
  void ParseChannelDelete   ( htsmsg_t *m );
  void ParseRecordingUpdate ( htsmsg_t *m );
  void ParseRecordingDelete ( htsmsg_t *m );
  void ParseEventUpdate     ( htsmsg_t *m );
  void ParseEventDelete     ( htsmsg_t *m );
  bool ParseEvent           ( htsmsg_t *msg, SEvent &evt );

public:
  /*
   * Connection (pass-thru)
   */
  bool WaitForConnection ( void )
  {
    PLATFORM::CLockObject lock(m_conn.Mutex());
    return m_conn.WaitForConnection();
  }
  inline const char *GetServerName    ( void )
  {
    return m_conn.GetServerName();
  }
  inline const char *GetServerVersion ( void )
  {
    return m_conn.GetServerVersion();
  }
  inline const char *GetServerString  ( void )
  {
    return m_conn.GetServerString();
  }
  inline bool HasCapability(const std::string &capability) const
  {
      return m_conn.HasCapability(capability);
  }
  inline bool IsConnected ( void ) const
  {
    return m_conn.IsConnected();
  }
  inline void Disconnect ( void )
  {
    m_conn.Disconnect();
  }

  /*
   * Demuxer (pass-thru)
   */
  inline bool         DemuxOpen           ( const PVR_CHANNEL &chn )
  {
    return m_dmx.Open(chn);
  }
  inline void         DemuxClose          ( void )
  {
    m_dmx.Close();
  }
  inline DemuxPacket *DemuxRead           ( void )
  {
    return m_dmx.Read();
  }
  inline void         DemuxFlush          ( void )
  {
    m_dmx.Flush();
  }
  inline void         DemuxAbort          ( void )
  {
    m_dmx.Abort();
  }
  inline bool         DemuxSeek           ( int time, bool backward, double *startpts )
  {
    return m_dmx.Seek(time, backward, startpts);
  }
  inline void         DemuxSpeed          ( int speed )
  {
    return m_dmx.Speed(speed);
  }
  inline PVR_ERROR    DemuxCurrentStreams ( PVR_STREAM_PROPERTIES *streams )
  {
    return m_dmx.CurrentStreams(streams);
  }
  inline PVR_ERROR    DemuxCurrentSignal  ( PVR_SIGNAL_STATUS &sig )
  {
    return m_dmx.CurrentSignal(sig);
  }
  inline time_t       DemuxGetTimeshiftTime() const
  {
    return m_dmx.GetTimeshiftTime();
  }

  /*
   * VFS (pass-thru)
   */
  inline bool         VfsOpen             ( const PVR_RECORDING &rec )
  {
    return m_vfs.Open(rec);
  }
  inline void         VfsClose            ( void )
  {
    PLATFORM::CLockObject lock(m_conn.Mutex());
    m_vfs.Close();
  }
  inline int          VfsRead             ( unsigned char *buf, unsigned int len )
  {
    return m_vfs.Read(buf, len);
  }
  inline long long    VfsSeek             ( long long position, int whence )
  {
    return m_vfs.Seek(position, whence);
  }
  inline long long    VfsTell             ( void )
  {
    return m_vfs.Tell();
  }
  inline long long    VfsSize             ( void )
  {
    return m_vfs.Size();
  }
};
