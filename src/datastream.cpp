/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  NMEA Data Stream Object
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************

 ***************************************************************************
 *  Parts of this file were adapted from source code found in              *
 *  John F. Waers (jfwaers@csn.net) public domain program MacGPS45         *
 ***************************************************************************
 *
 */
#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "wx/tokenzr.h"
#include <wx/datetime.h>

#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "dychart.h"

#include "datastream.h"

#define SERIAL_OVERLAPPED

extern int              g_nNMEADebug;
extern bool             g_bGPSAISMux;
extern int              g_total_NMEAerror_messages;




#if !defined(NAN)
static const long long lNaN = 0xfff8000000000000;
#define NAN (*(double*)&lNaN)
#endif

//------------------------------------------------------------------------------
//    OCPN_DataStreamEvent Event Implementation
//------------------------------------------------------------------------------

const wxEventType wxEVT_OCPN_DATASTREAM = wxNewEventType();

OCPN_DataStreamEvent::OCPN_DataStreamEvent( wxEventType commandType, int id )
      :wxEvent(id, commandType)
{
}


OCPN_DataStreamEvent::~OCPN_DataStreamEvent( )
{
}

wxEvent* OCPN_DataStreamEvent::Clone() const
{
    OCPN_DataStreamEvent *newevent=new OCPN_DataStreamEvent(*this);
    newevent->m_NMEAstring=this->m_NMEAstring.c_str();  // this enforces a deep copy of the string data
    newevent->m_datasource=this->m_datasource.c_str();  
    return newevent;
}


//------------------------------------------------------------------------------
//    DataStream Implementation
//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(DataStream, wxEvtHandler)

//           EVT_SOCKET(SOCKET_ID, DataStream::OnSocketEvent)
//  EVT_TIMER(TIMER_NMEA1, DataStream::OnTimerNMEA)

  END_EVENT_TABLE()

// constructor
DataStream::DataStream(wxEvtHandler *input_consumer,
             const wxString& Port,
             const wxString& BaudRate,
             dsPortType io_select,
             int priority,
             int EOS_type,
             int handshake_type,
             void *user_data )
  
{
    m_consumer = input_consumer;
    m_portstring = Port;
    m_BaudRate = BaudRate;
    m_io_select = io_select;
    m_priority = priority;
    m_handshake = handshake_type;
    m_user_data = user_data;
    
    Init();
    
    Open();
}

void DataStream::Init(void)
{
    m_pSecondary_Thread = NULL;
    m_bok = false;
    SetSecThreadInActive();
    m_Thread_run_flag = -1;
    
}

void DataStream::Open(void) 
{
    //  Open an input port
    if( (m_io_select == DS_TYPE_INPUT) || (m_io_select == DS_TYPE_INPUT_OUTPUT) ) {
        
        //    Data Source is specified serial port
        if(m_portstring.Contains(_T("Serial")))
        {
            wxString comx;
            comx =  m_portstring.AfterFirst(':');      // strip "Serial:"
        
 #ifdef __WXMSW__
            wxString scomx = comx;
            scomx.Prepend(_T("\\\\.\\"));                  // Required for access to Serial Ports greater than COM9
        
        //  As a quick check, verify that the specified port is available
            HANDLE hSerialComm = CreateFile(scomx.fn_str(),       // Port Name
                                          GENERIC_READ,
                                          0,
                                          NULL,
                                          OPEN_EXISTING,
                                          0,
                                          NULL);
        
            if(hSerialComm == INVALID_HANDLE_VALUE) {
                m_last_error = DS_ERROR_PORTNOTFOUND;
                return;
            }
        
            else
                CloseHandle(hSerialComm);
#endif        
        
        //    Kick off the DataSource RX thread
            m_pSecondary_Thread = new OCP_DataStreamInput_Thread(this, m_consumer, comx, m_BaudRate);
            m_Thread_run_flag = 1;
            m_pSecondary_Thread->Run();
            
            m_bok = true;
        }
    }
            
}


DataStream::~DataStream()
{
}

void DataStream::Close()
{
//    Kill off the Secondary RX Thread if alive
      if(m_pSecondary_Thread)
      {
          if(m_bsec_thread_active)              // Try to be sure thread object is still alive
          {
                wxLogMessage(_T("Stopping Secondary Thread"));

                m_Thread_run_flag = 0;
                int tsec = 5;
                while((m_Thread_run_flag >= 0) && (tsec--))
                {
                    wxSleep(1);
                }

                wxString msg;
                if(m_Thread_run_flag < 0)
                      msg.Printf(_T("Stopped in %d sec."), 5 - tsec);
                else
                     msg.Printf(_T("Not Stopped after 5 sec."));
                wxLogMessage(msg);

          }

          m_pSecondary_Thread = NULL;
          m_bsec_thread_active = false;
      }
}


void DataStream::OnTimerNMEA(wxTimerEvent& event)
{
}



//-------------------------------------------------------------------------------------------------------------
//
//    Serial Input Thread
//
//    This thread manages reading the data stream from the declared serial port
//
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
//    OCP_DataStreamInput_Thread Implementation
//-------------------------------------------------------------------------------------------------------------

OCP_DataStreamInput_Thread::OCP_DataStreamInput_Thread(DataStream *Launcher,
                                                       wxEvtHandler *MessageTarget,
                                                       const wxString& PortName,
                                                       const wxString& strBaudRate )
{
      m_launcher = Launcher;                        // This thread's immediate "parent"

      m_pMessageTarget = MessageTarget;

      m_PortName = PortName;


//      m_pPortMutex = pPortMutex;

      rx_buffer = new char[DS_RX_BUFFER_SIZE + 1];
      temp_buf = new char[DS_RX_BUFFER_SIZE + 1];

      put_ptr = rx_buffer;                            // local circular queue
      tak_ptr = rx_buffer;

//      m_pShareMutex = pMutex;                         // IPC buffer
//      rx_share_buffer_state = RX_BUFFER_EMPTY;


      m_baud = 4800;                    // default
      long lbaud;
      if(strBaudRate.ToLong(&lbaud))
          m_baud = (int)lbaud;
      
      
      Create();
}

OCP_DataStreamInput_Thread::~OCP_DataStreamInput_Thread(void)
{
      delete[] rx_buffer;
      delete[] temp_buf;
}

void OCP_DataStreamInput_Thread::OnExit(void)
{
}

//      Sadly, the thread itself must implement the underlying OS serial port
//      in a very machine specific way....

#ifdef __POSIX__
//    Entry Point
void *OCP_DataStreamInput_Thread::Entry()
{
      m_launcher->SetSecThreadActive();               // I am alive
    
      bool not_done = true;
      bool nl_found;
      wxString msg;


      //    Request the com port from the comm manager
      if ((m_gps_fd = OpenComPortPhysical(m_PortName, m_baud)) < 0)
      {
            wxString msg(_T("NMEA input device open failed: "));
            msg.Append(m_PortName);
            ThreadMessage(msg);
            goto thread_exit;
      }

#if 0      
      if(wxMUTEX_NO_ERROR != m_pPortMutex->Lock())              // I have the ball
      {
            wxString msg(_T("NMEA input device failed to lock Mutex on port : "));
            msg.Append(m_PortName);
//            ThreadMessage(msg);
            goto thread_exit;
      }

#endif


//    The main loop

    while((not_done) && (m_launcher->m_Thread_run_flag > 0))
    {
        if(TestDestroy())
            not_done = false;                               // smooth exit

#if 0            
        if( m_launcher->IsPauseRequested())                 // external pause requested?
        {
              CloseComPortPhysical(m_gps_fd);
              m_pPortMutex->Unlock();                       // release the port

              wxThread::Sleep(2000);                        // stall for a bit

              //  Now try to regain the Mutex
              while(wxMUTEX_BUSY == m_pPortMutex->TryLock()){};

              //  Re-initialize the port
              if ((m_gps_fd = OpenComPortPhysical(m_PortName, m_baud)) < 0)
              {
                    wxString msg(_T("NMEA input device open failed after requested Pause on port: "));
                    msg.Append(m_PortName);
//                    ThreadMessage(msg);
                    goto thread_exit;
              }

        }
#endif
      //    Blocking, timeout protected read of one character at a time
      //    Timeout value is set by c_cc[VTIME]
      //    Storing incoming characters in circular buffer
      //    And watching for new line character
      //     On new line character, send notification to parent

            char next_byte = 0;
            ssize_t newdata;
            newdata = read(m_gps_fd, &next_byte, 1);            // read of one char
                                                                  // return (-1) if no data available, timeout

      #ifdef __WXOSX__
            if (newdata < 0 )
                  wxThread::Sleep(100) ;
      #endif


      // Fulup patch for handling hot-plug or wakeup events
      // from serial port drivers
            {
                  static int maxErrorLoop;

                  if (newdata > 0)
                  {
            // we have data, so clear error
                        maxErrorLoop =0;
                  }
                  else
                  {
            // no need to retry every 1ms when on error
                        sleep (1);

            // if we have more no character for 5 second then try to reopen the port
                        if (maxErrorLoop++ > 5)
                        {

            // do not retry for the next 5s
                              maxErrorLoop = 0;

            //  Turn off Open/Close logging
//                              bool blog = m_pCommMan->GetLogFlag();
//                              m_pCommMan->SetLogFlag(false);

            // free old unplug current port
                              CloseComPortPhysical(m_gps_fd);

            //    Request the com port from the comm manager
                              if ((m_gps_fd = OpenComPortPhysical(m_PortName, m_baud)) < 0)  {
                                    wxString msg(_T("NMEA input device open failed (will retry): "));
                                    msg.Append(m_PortName);
//                                    ThreadMessage(msg);
                              } else {
                                    wxString msg(_T("NMEA input device open on hotplug OK: "));
                              }

            //      Reset the log flag
//                              m_pCommMan->SetLogFlag(blog);
                        }
                  }
            } // end Fulup hack


            //  And process any character

            if(newdata > 0)
            {
//                    printf("%c", next_byte);

                  nl_found = false;

                  *put_ptr++ = next_byte;
                  if((put_ptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                  put_ptr = rx_buffer;

                  if(0x0a == next_byte)
                  nl_found = true;


      //    Found a NL char, thus end of message?
                  if(nl_found)
                  {
                  char *tptr;
                  char *ptmpbuf;


      //    Copy the message into a temporary _buffer

                  tptr = tak_ptr;
                  ptmpbuf = temp_buf;

                  while((*tptr != 0x0a) && (tptr != put_ptr))
                  {
                        *ptmpbuf++ = *tptr++;

                        if((tptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                        tptr = rx_buffer;

                        wxASSERT_MSG((ptmpbuf - temp_buf) < DS_RX_BUFFER_SIZE, (const wxChar *)"temp_buf overrun1");

                  }

                  if((*tptr == 0x0a) && (tptr != put_ptr))    // well formed sentence
                  {
                        *ptmpbuf++ = *tptr++;
                        if((tptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                        tptr = rx_buffer;

                        wxASSERT_MSG((ptmpbuf - temp_buf) < DS_RX_BUFFER_SIZE, (const wxChar *)"temp_buf overrun2");

                        *ptmpbuf = 0;

                        tak_ptr = tptr;

      //    Message is ready to parse and send out
                        wxString str_temp_buf(temp_buf, wxConvUTF8);
                        Parse_And_Send_Posn(str_temp_buf);
                  }

                  }                   //if nl
            }                       // if newdata > 0
      //              ThreadMessage(_T("Timeout 1"));

    }                          // the big while...


//          Close the port cleanly
    CloseComPortPhysical(m_gps_fd);



thread_exit:
    m_launcher->SetSecThreadInActive();             // I am dead
    m_launcher->m_Thread_run_flag = -1;

    return 0;
}


#endif          //__POSIX__


#ifdef __WXMSW__

//    Entry Point
void *OCP_DataStreamInput_Thread::Entry()
{
      wxString msg;

      m_launcher->SetSecThreadActive();               // I am alive
      
      OVERLAPPED osReader = {0};

      bool not_done;
      BOOL fWaitingOnRead = FALSE;
      HANDLE hSerialComm = (HANDLE)(-1);

           //    Request the com port from the comm manager
      if ((m_gps_fd = OpenComPortPhysical(m_PortName, m_baud)) < 0)
      {
            wxString msg(_T("NMEA input device open failed: "));
            msg.Append(m_PortName);
            wxString msg_error;
            msg_error.Printf(_T("...GetLastError():  %d"), GetLastError());
            msg.Append(msg_error);

            ThreadMessage(msg);
            goto thread_exit;
      }

      hSerialComm = (HANDLE)m_gps_fd;


#ifdef SERIAL_OVERLAPPED
//    Set up read event specification

      if(!SetCommMask((HANDLE)m_gps_fd, EV_RXCHAR)) // Setting Event Type
      {
            wxString msg(_T("NMEA input device (overlapped) SetCommMask failed: "));
            msg.Append(m_PortName);
            wxString msg_error;
            msg_error.Printf(_T("...GetLastError():  %d"), GetLastError());
            msg.Append(msg_error);

            ThreadMessage(msg);
            goto thread_exit;
      }

// Create the overlapped event. Must be closed before exiting
// to avoid a handle leak.
      osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

      if (osReader.hEvent == NULL)
      {
            wxString msg(_T("NMEA input device (overlapped) CreateEvent failed: "));
            msg.Append(m_PortName);
            wxString msg_error;
            msg_error.Printf(_T("...GetLastError():  %d"), GetLastError());
            msg.Append(msg_error);

            ThreadMessage(msg);
            goto thread_exit;
      }

#if 0
      if(wxMUTEX_NO_ERROR != m_pPortMutex->Lock())              // I have the ball
      {
            wxString msg(_T("NMEA input device failed to lock Mutex on port : "));
            msg.Append(m_PortName);
            ThreadMessage(msg);
            goto thread_exit;
      }
#endif
      not_done = true;
      bool nl_found = false;

#define READ_BUF_SIZE 20
      char szBuf[READ_BUF_SIZE];

      DWORD dwRead;

      m_n_timeout = 0;                // reset the timeout counter
      int n_reopen_wait = 0;

//    The main loop

      while((not_done) && (m_launcher->m_Thread_run_flag > 0))
      {
            if(TestDestroy())
            {
                  not_done = false;                               // smooth exit
                  goto thread_exit;                               // smooth exit
            }

            //    Was port closed due to error condition?
            while(!m_gps_fd)
            {
                  if((TestDestroy()) || (m_launcher->m_Thread_run_flag == 0))
                        goto thread_exit;                               // smooth exit

                  if(n_reopen_wait)
                  {
                        wxThread::Sleep(n_reopen_wait);                        // stall for a bit
                        n_reopen_wait = 0;
                  }


                  if ((m_gps_fd = OpenComPortPhysical(m_PortName, m_baud)) > 0)
                  {
                        hSerialComm = (HANDLE)m_gps_fd;
                        m_n_timeout = 0;                             // reset the timeout counter

                  }
                  else
                  {
                        m_gps_fd = NULL;
                        wxThread::Sleep(2000);                        // stall for a bit
                  }

            }

#if 0            
            if( m_launcher->IsPauseRequested())                 // external pause requested?
            {
                  m_pCommMan->CloseComPort(m_gps_fd);
                  m_pPortMutex->Unlock();                       // release the port

                  wxThread::Sleep(2000);                        // stall for a bit

              //  Now try to regain the Mutex
                  while(wxMUTEX_BUSY == m_pPortMutex->TryLock()){};

              //  Re-initialize the port
                  if ((m_gps_fd = OpenComPortPhysical(m_PortName, m_baud)) > 0)
                  {
                        hSerialComm = (HANDLE)m_gps_fd;
                        m_n_timeout = 0;                           // reset the timeout counter

                  }
                  else
                  {
                        wxString msg(_T("NMEA input device open failed after requested Pause on port: "));
                        msg.Append(m_PortName);
                        ThreadMessage(msg);
                        goto thread_exit;
                  }
            }

#endif
            if (!fWaitingOnRead)
            {
   // Issue read operation.
                if (!ReadFile(hSerialComm, szBuf, READ_BUF_SIZE, &dwRead, &osReader))
                {
                    int errt = GetLastError();

                    // read delayed ?
                    if (errt == ERROR_IO_PENDING)
                    {
                          fWaitingOnRead = TRUE;
                    }

                    //  We sometimes see this return if using virtual ports, with no data flow....
                    //  Especially on Xport 149 and above....
                    //  What does it mean?  Who knows...
                    //  Workaround:  Try the IO again after a slight pause
                    else if(errt == ERROR_NO_SYSTEM_RESOURCES)
                    {
                          dwRead = 0;
                          nl_found = false;
                          fWaitingOnRead = FALSE;
                          wxThread::Sleep(1000);                        // stall for a bit

                    }
                    else                              // reset the port and retry on any other error
                    {
                          CloseComPortPhysical(m_gps_fd);
                          m_gps_fd = NULL;
                          dwRead = 0;
                          nl_found = false;
                          fWaitingOnRead = FALSE;
                          n_reopen_wait = 2000;
                    }
                }
                else
                {      // read completed immediately
                    goto HandleASuccessfulRead;
                }
            }


            // Read command has been issued, and did not return immediately

#define READ_TIMEOUT      2000      // milliseconds

            DWORD dwRes;

            if (fWaitingOnRead)
            {
                  //    Loop forever, checking for thread exit request
                  while(fWaitingOnRead)
                  {
                        if((TestDestroy()) || (m_launcher->m_Thread_run_flag == 0))
                            goto fail_point;                               // smooth exit


                        dwRes = WaitForSingleObject(osReader.hEvent, READ_TIMEOUT);
                        switch(dwRes)
                        {
                              case WAIT_OBJECT_0:
                                    if (!GetOverlappedResult(hSerialComm, &osReader, &dwRead, FALSE))
                                    {
                                          CloseComPortPhysical(m_gps_fd);
                                          m_gps_fd = NULL;
                                          dwRead = 0;
                                          nl_found = false;
                                          fWaitingOnRead = FALSE;
                                          n_reopen_wait = 2000;

                                    }
                                    else
                                    {
                                           goto HandleASuccessfulRead;             // Read completed successfully.
                                    }
                                    break;

                              case WAIT_TIMEOUT:
                                    if((g_total_NMEAerror_messages < g_nNMEADebug) && (g_nNMEADebug > 1000))
                                    {
                                          g_total_NMEAerror_messages++;
                                          wxString msg;
                                          msg.Printf(_T("NMEA timeout"));
                                          ThreadMessage(msg);
                                    }

                                    m_n_timeout++;
                                    if(m_n_timeout > 5)             //5 x 2000 msec
                                    {
                                          dwRead = 0;
                                          nl_found = false;
                                          fWaitingOnRead = FALSE;
                                    }
                                    break;

                              default:                // Reset the port on all unhandled return values....
                                    CloseComPortPhysical(m_gps_fd);
                                    m_gps_fd = NULL;
                                    dwRead = 0;
                                    nl_found = false;
                                    fWaitingOnRead = FALSE;
                                    n_reopen_wait = 2000;

                                    break;
                        }     // switch
                  }           // while
            }                 // if




HandleASuccessfulRead:

            if(dwRead > 0)
            {
                 m_n_timeout = 0;                // reset the timeout counter
                 if((g_total_NMEAerror_messages < g_nNMEADebug) && (g_nNMEADebug > 1000))
                  {
                        g_total_NMEAerror_messages++;
                        wxString msg;
                        msg.Printf(_T("NMEA activity...%d bytes"), dwRead);
                        ThreadMessage(msg);
                  }

                  int nchar = dwRead;
                  char *pb = szBuf;

                  while(nchar)
                  {
                        if(0x0a == *pb)
                              nl_found = true;

                        *put_ptr++ = *pb++;
                        if((put_ptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                              put_ptr = rx_buffer;

                        nchar--;
                  }
                  if((g_total_NMEAerror_messages < g_nNMEADebug) && (g_nNMEADebug > 1000))
                  {
                        g_total_NMEAerror_messages++;
                        wxString msg1 = _T("Buffer is: ");
                        int nc = dwRead;
                        char *pb = szBuf;
                        while(nc)
                        {
                              msg1.Append(*pb++);
                              nc--;
                        }
                        ThreadMessage(msg1);
                  }
            }


//    Found a NL char, thus end of message?
            if(nl_found)
            {
                  char *tptr;
                  char *ptmpbuf;

                  bool partial = false;
                  while (!partial)
                  {

            //    Copy the message into a temp buffer

                        tptr = tak_ptr;
                        ptmpbuf = temp_buf;

                        while((*tptr != 0x0a) && (tptr != put_ptr))
                        {
                              *ptmpbuf++ = *tptr++;

                              if((tptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                                    tptr = rx_buffer;
                              wxASSERT_MSG((ptmpbuf - temp_buf) < DS_RX_BUFFER_SIZE, _T("temp_buf overrun"));
                        }

                        if((*tptr == 0x0a) && (tptr != put_ptr))    // well formed sentence
                        {
                              *ptmpbuf++ = *tptr++;
                              if((tptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                                    tptr = rx_buffer;
                              wxASSERT_MSG((ptmpbuf - temp_buf) < DS_RX_BUFFER_SIZE, _T("temp_buf overrun"));

                              *ptmpbuf = 0;

                              tak_ptr = tptr;

      // parse and send the message
                              wxString str_temp_buf(temp_buf, wxConvUTF8);
                              Parse_And_Send_Posn(str_temp_buf);
                        }
                        else
                        {
                              partial = true;
                        }
                }                 // while !partial

            }           // nl found

       fWaitingOnRead = FALSE;

      }           // the big while...



fail_point:
thread_exit:

//          Close the port cleanly
      CloseComPortPhysical(m_gps_fd);

      if (osReader.hEvent)
            CloseHandle(osReader.hEvent);

      m_launcher->SetSecThreadInActive();             // I am dead
      m_launcher->m_Thread_run_flag = -1;
      
      return 0;

#else                   // non-overlapped
//    Set up read event specification

      if(!SetCommMask((HANDLE)m_gps_fd, EV_RXCHAR)) // Setting Event Type
            goto thread_exit;

      not_done = true;
      bool nl_found;

#define READ_BUF_SIZE 20
      char szBuf[READ_BUF_SIZE];

      DWORD dwRead;
      DWORD dwOneRead;
      DWORD dwCommEvent;
      char  chRead;
      int ic;

//    The main loop

      while((not_done) && (m_launcher->m_Thread_run_flag > 0))
      {
            if(TestDestroy())
                  not_done = false;                               // smooth exit


            dwRead = 0;
            ic = 0;

            if (WaitCommEvent((HANDLE)m_gps_fd, &dwCommEvent, NULL))
            {
                do {
                      if (ReadFile((HANDLE)m_gps_fd, &chRead, 1, &dwOneRead, NULL))
                      {
                            szBuf[ic] = chRead;
                            dwRead++;
                            if(ic++ > READ_BUF_SIZE - 1)
                                  goto HandleASuccessfulRead;
                      }
                      else
                      {            // An error occurred in the ReadFile call.
                            goto fail_point;                               // smooth exit

                      }
                } while (dwOneRead);
            }
            else
                  goto fail_point;                               // smooth exit



HandleASuccessfulRead:

            if(dwRead > 0)
            {
                  if((g_total_NMEAerror_messages < g_nNMEADebug) && (g_nNMEADebug > 1000))
                  {
                        g_total_NMEAerror_messages++;
                        wxString msg;
                        msg.Printf(_T("NMEA activity...%d bytes"), dwRead);
                        ThreadMessage(msg);
                  }

                  int nchar = dwRead;
                  char *pb = szBuf;

                  while(nchar)
                  {
                        if(0x0a == *pb)
                              nl_found = true;

                        *put_ptr++ = *pb++;
                        if((put_ptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                              put_ptr = rx_buffer;

                        nchar--;
                  }
                  if((g_total_NMEAerror_messages < g_nNMEADebug) && (g_nNMEADebug > 1000))
                  {
                        g_total_NMEAerror_messages++;
                        wxString msg1 = _T("Buffer is: ");
                        int nc = dwRead;
                        char *pb = szBuf;
                        while(nc)
                        {
                              msg1.Append(*pb++);
                              nc--;
                        }
                        ThreadMessage(msg1);
                  }
            }

//    Found a NL char, thus end of message?
            if(nl_found)
            {
                  char *tptr;
                  char *ptmpbuf;

                  bool partial = false;
                  while (!partial)
                  {

            //    Copy the message into a temp buffer

                        tptr = tak_ptr;
                        ptmpbuf = temp_buf;

                        while((*tptr != 0x0a) && (tptr != put_ptr))
                        {
                              *ptmpbuf++ = *tptr++;

                              if((tptr - rx_buffer) > RX_BUFFER_SIZE)
                                    tptr = rx_buffer;
                              wxASSERT_MSG((ptmpbuf - temp_buf) < DS_RX_BUFFER_SIZE, "temp_buf overrun");
                        }

                        if((*tptr == 0x0a) && (tptr != put_ptr))    // well formed sentence
                        {
                              *ptmpbuf++ = *tptr++;
                              if((tptr - rx_buffer) > DS_RX_BUFFER_SIZE)
                                    tptr = rx_buffer;
                              wxASSERT_MSG((ptmpbuf - temp_buf) < DS_RX_BUFFER_SIZE, "temp_buf overrun");

                              *ptmpbuf = 0;

                              tak_ptr = tptr;

      // parse and send the message
                              if(g_bShowOutlines)
                              {
                                    wxString str_temp_buf(temp_buf, wxConvUTF8);
                                    Parse_And_Send_Posn(str_temp_buf);
                              }
                        }
                        else
                        {
                              partial = true;
                        }
                  }                 // while !partial


            }           // nl found
      }           // the big while...



fail_point:
thread_exit:

//          Close the port cleanly
      CloseComPortPhysical(m_gps_fd);

      m_launcher->SetSecThreadInActive();             // I am dead
      m_launcher->m_Thread_run_flag = -1;
      
      return 0;





#endif
}

#endif            // __WXMSW__


void OCP_DataStreamInput_Thread::Parse_And_Send_Posn(wxString &str_temp_buf)
{
      if( g_nNMEADebug && (g_total_NMEAerror_messages < g_nNMEADebug) )
      {
            g_total_NMEAerror_messages++;
            wxString msg(_T("NMEA Sentence received..."));
            msg.Append(str_temp_buf);
//            ThreadMessage(msg);
      }


      OCPN_DataStreamEvent Nevent(wxEVT_OCPN_DATASTREAM, 0);
      Nevent.SetNMEAString(str_temp_buf);
      m_pMessageTarget->AddPendingEvent(Nevent);

      return;
}


void OCP_DataStreamInput_Thread::ThreadMessage(const wxString &msg)
{

    //    Signal the main program thread
      wxCommandEvent event( EVT_THREADMSG,  GetId());
      event.SetEventObject( (wxObject *)this );
      event.SetString(msg);
      m_pMessageTarget->AddPendingEvent(event);

}




#ifdef __POSIX__

int OCP_DataStreamInput_Thread::OpenComPortPhysical(wxString &com_name, int baud_rate)
{

    // Declare the termios data structures
      termios ttyset_old;
      termios ttyset;

    // Open the serial port.
      int com_fd;
      if ((com_fd = open(com_name.mb_str(), O_RDWR|O_NONBLOCK|O_NOCTTY)) < 0)
//      if ((com_fd = open(com_name.mb_str(), O_RDWR|O_NOCTTY)) < 0)
            return com_fd;


      speed_t baud_parm;
      switch(baud_rate)
      {
            case 4800:
                  baud_parm = B4800;
                  break;
            case 9600:
                  baud_parm = B9600;
                  break;
            case 38400:
                  baud_parm = B38400;
                  break;
            default:
                  baud_parm = B4800;
                  break;
      }



     if (isatty(com_fd) != 0)
      {
            /* Save original terminal parameters */
            if (tcgetattr(com_fd,&ttyset_old) != 0)
                  return -128;

            memcpy(&ttyset, &ttyset_old, sizeof(termios));

      //  Build the new parms off the old

      //  Baud Rate
            cfsetispeed(&ttyset, baud_parm);
            cfsetospeed(&ttyset, baud_parm);

            tcsetattr(com_fd, TCSANOW, &ttyset);

      // Set blocking/timeout behaviour
            memset(ttyset.c_cc,0,sizeof(ttyset.c_cc));
            ttyset.c_cc[VTIME] = 5;                        // 0.5 sec timeout
            fcntl(com_fd, F_SETFL, fcntl(com_fd, F_GETFL) & !O_NONBLOCK);

      // No Flow Control

            ttyset.c_cflag &= ~(PARENB | PARODD | CRTSCTS);
            ttyset.c_cflag |= CREAD | CLOCAL;
            ttyset.c_iflag = ttyset.c_oflag = ttyset.c_lflag = (tcflag_t) 0;

            int stopbits = 1;
            char parity = 'N';
            ttyset.c_iflag &=~ (PARMRK | INPCK);
            ttyset.c_cflag &=~ (CSIZE | CSTOPB | PARENB | PARODD);
            ttyset.c_cflag |= (stopbits==2 ? CS7|CSTOPB : CS8);
            switch (parity)
            {
                  case 'E':
                        ttyset.c_iflag |= INPCK;
                        ttyset.c_cflag |= PARENB;
                        break;
                  case 'O':
                        ttyset.c_iflag |= INPCK;
                        ttyset.c_cflag |= PARENB | PARODD;
                        break;
            }
            ttyset.c_cflag &=~ CSIZE;
            ttyset.c_cflag |= (CSIZE & (stopbits==2 ? CS7 : CS8));
            if (tcsetattr(com_fd, TCSANOW, &ttyset) != 0)
                  return -129;

            tcflush(com_fd, TCIOFLUSH);
      }

      return com_fd;
}


int OCP_DataStreamInput_Thread::CloseComPortPhysical(int fd)
{

      close(fd);

      return 0;
}


int OCP_DataStreamInput_Thread::WriteComPortPhysical(int port_descriptor, const wxString& string)
{
      ssize_t status;
      status = write(port_descriptor, string.mb_str(), string.Len());

      return status;
}

int OCP_DataStreamInput_Thread::WriteComPortPhysical(int port_descriptor, unsigned char *msg, int count)
{
      ssize_t status;
      status = write(port_descriptor, msg, count);

      return status;
}

int OCP_DataStreamInput_Thread::ReadComPortPhysical(int port_descriptor, int count, unsigned char *p)
{
//    Blocking, timeout protected read of one character at a time
//    Timeout value is set by c_cc[VTIME]

      return read(port_descriptor, p, count);            // read of (count) characters
}


bool OCP_DataStreamInput_Thread::CheckComPortPhysical(int port_descriptor)
{
      fd_set rec;
      struct timeval t;
//      posix_serial_data *psd = (posix_serial_data *)port_descriptor;
//      int fd = psd->fd;

      int fd = port_descriptor;
      FD_ZERO(&rec);
      FD_SET(fd,&rec);

      t.tv_sec  = 0;
      t.tv_usec = 1000;
      (void) select(fd+1,&rec,NULL,NULL,&t);
      if(FD_ISSET(fd,&rec))
            return true;

      return false;
}


#endif            // __POSIX__

#ifdef __WXMSW__
int OCP_DataStreamInput_Thread::OpenComPortPhysical(wxString &com_name, int baud_rate)
{

//    Set up the serial port
      wxString xcom_name = com_name;
      xcom_name.Prepend(_T("\\\\.\\"));                  // Required for access to Serial Ports greater than COM9

#ifdef SERIAL_OVERLAPPED
      DWORD open_flags = FILE_FLAG_OVERLAPPED;
#else
      DWORD open_flags = 0;
#endif

      HANDLE hSerialComm = CreateFile(xcom_name.fn_str(),      // Port Name
                                 GENERIC_READ | GENERIC_WRITE,     // Desired Access
                                 0,                               // Shared Mode
                                 NULL,                            // Security
                                 OPEN_EXISTING,             // Creation Disposition
                                 open_flags,
                                 NULL);

      if(hSerialComm == INVALID_HANDLE_VALUE)
      {
            return (0 - abs((int)::GetLastError()));
      }

      if(!SetupComm(hSerialComm, 1024, 1024))
      {
            return (0 - abs((int)::GetLastError()));
      }


      DCB dcbConfig;

      if(GetCommState(hSerialComm, &dcbConfig))           // Configuring Serial Port Settings
      {
            dcbConfig.BaudRate = baud_rate;
            dcbConfig.ByteSize = 8;
            dcbConfig.Parity = NOPARITY;
            dcbConfig.StopBits = ONESTOPBIT;
            dcbConfig.fBinary = TRUE;
            dcbConfig.fRtsControl = RTS_CONTROL_ENABLE;
            dcbConfig.fDtrControl = DTR_CONTROL_ENABLE;
            dcbConfig.fOutxDsrFlow = false;
            dcbConfig.fOutxCtsFlow = false;
            dcbConfig.fDsrSensitivity = false;
            dcbConfig.fOutX = false;
	      dcbConfig.fInX = false;
	      dcbConfig.fInX = false;
	      dcbConfig.fInX = false;
	}

      else
      {
            return (0 - abs((int)::GetLastError()));
      }

      if(!SetCommState(hSerialComm, &dcbConfig))
      {
            return (0 - abs((int)::GetLastError()));
      }

      COMMTIMEOUTS commTimeout;
      int TimeOutInSec = 2;
      commTimeout.ReadIntervalTimeout = 1000*TimeOutInSec;
      commTimeout.ReadTotalTimeoutConstant = 1000*TimeOutInSec;
      commTimeout.ReadTotalTimeoutMultiplier = 0;
      commTimeout.WriteTotalTimeoutConstant = 1000*TimeOutInSec;
      commTimeout.WriteTotalTimeoutMultiplier = 0;


      if(!SetCommTimeouts(hSerialComm, &commTimeout))
      {
            return (0 - abs((int)::GetLastError()));
      }


      return (int)hSerialComm;
}

int OCP_DataStreamInput_Thread::CloseComPortPhysical(int fd)
{
      if((HANDLE)fd != INVALID_HANDLE_VALUE)
            CloseHandle((HANDLE)fd);
      return 0;
}

int OCP_DataStreamInput_Thread::WriteComPortPhysical(int port_descriptor, const wxString& string)
{
      unsigned int dwSize = string.Len();
      char *pszBuf = (char *)malloc((dwSize + 1) * sizeof(char));
      strncpy(pszBuf, string.mb_str(), dwSize+1);

#ifdef SERIAL_OVERLAPPED

      OVERLAPPED osWrite = {0};
      DWORD dwWritten;
      int fRes;

   // Create this writes OVERLAPPED structure hEvent.
      osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
      if (osWrite.hEvent == NULL)
      {
      // Error creating overlapped event handle.
            free(pszBuf);
            return 0;
      }

   // Issue write.
      if (!WriteFile((HANDLE)port_descriptor, pszBuf, dwSize, &dwWritten, &osWrite))
      {
            if (GetLastError() != ERROR_IO_PENDING)
            {
         // WriteFile failed, but it isn't delayed. Report error and abort.
                  fRes = 0;
            }
            else
            {
         // Write is pending.
                  if (!GetOverlappedResult((HANDLE)port_descriptor, &osWrite, &dwWritten, TRUE))
                        fRes = 0;
                  else
            // Write operation completed successfully.
                        fRes = dwWritten;
            }
      }
      else
      // WriteFile completed immediately.
            fRes = dwWritten;

      CloseHandle(osWrite.hEvent);

      free (pszBuf);

#else
      DWORD dwWritten;
      int fRes;

        // Issue write.
      if (!WriteFile((HANDLE)port_descriptor, pszBuf, dwSize, &dwWritten, NULL))
            fRes = 0;         // WriteFile failed, . Report error and abort.
      else
            fRes = dwWritten;      // WriteFile completed immediately.

      free (pszBuf);

#endif

      return fRes;
}

int OCP_DataStreamInput_Thread::WriteComPortPhysical(int port_descriptor, unsigned char *msg, int count)
{
      return 0;
}

int OCP_DataStreamInput_Thread::ReadComPortPhysical(int port_descriptor, int count, unsigned char *p)
{
      return 0;
}


bool OCP_DataStreamInput_Thread::CheckComPortPhysical(int port_descriptor)
{
      return false;
}




#endif            // __WXMSW__


