/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  About Dialog
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
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
 *
 *
 *
 */

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"


#ifndef WX_PRECOMP
      #include <wx/wx.h>
#endif

#include "wx/textfile.h"
#include <wx/html/htmlwin.h>
#include <version.h> //Gunther

#include "about.h"
#include "chart1.h"
#include "chcanv.h"
#include "styles.h"

//    Some constants

//Gunther Start
wxString str_version_start = wxT("\n      Version ");
wxString str_version_major = wxString::Format(wxT("%i"),VERSION_MAJOR);
wxString str_version_minor = wxString::Format(wxT("%i"),VERSION_MINOR);
wxString str_version_patch = wxString::Format(wxT("%i"),VERSION_PATCH);
wxString str_version_date(VERSION_DATE, wxConvUTF8);
wxString OpenCPNVersion = str_version_start + str_version_major + wxT(".") + str_version_minor + wxT(".") + str_version_patch + wxT(" Build ") + str_version_date;
//Gunther End

extern wxString        *pHome_Locn;
extern wxString         glog_file;
extern wxString         gConfig_File;
extern ocpnStyle::StyleManager* g_StyleManager;

char AboutText[] =
{
  "\n                                         OpenCPN\n\n\
                       (c) 2000-2011 The OpenCPN Authors\n"
};

char OpenCPNInfo[] = {"\n\n\
      OpenCPN is a Free Software project, built by sailors.\n\
       It is freely available to download and distribute\n\
               without charge at Opencpn.org.\n\n\
       If you use OpenCPN, please consider contributing\n\
                or donating funds to the project.\n\n\
      Documentation\n\
           http://Opencpn.org\n\n"
};

char AuthorText[] =
{
"   David S Register\n\
      OpenCPN Lead Developer\n\n\
    Jesper Weissglas\n\
      Vector Chart Renderer Enhancements\n\
      User Interface Style\n\n\
    Sean D'Epagnier\n\
      OpenGL Architecture\n\n\
    Kathleen Boswell\n\
      Icon design\n\n\
    Flavius Bindea\n\
      CM93 Offset and AIS enhancements\n\n\
    Gunther Pilz\n\
      Windows Installer enhancements\n\n\
    Alan Bleasby\n\
      Garmin jeeps module\n\n\
    Jean-Eudes Onfray\n\
      Dashboard and Dialog enhancements\n\n\
    Pavel Kalian\n\
      S52 Rasterization Improvements\n\n\
    Piotr Carlson\n\
      General usability enhancements\n\n\
    Anders Lund\n\
      RouteManagerDialog\n\n\
    Gordon Mau\n\
      OpenCPN Documentation\n\n\
    Tim Francis\n\
      OpenCPN Documentation\n\n\
    Mark A Sikes\n\
      OpenCPN CoDeveloper\n\n\
    Thomas Haller\n\
      GPX Import/Export Implementation\n\n\
    Will Kamp\n\
      Toolbar Icon design\n\n\
    Richard Smith\n\
      OpenCPN CoDeveloper, MacOSX\n\n\
    David Herring\n\
      OpenCPN CoDeveloper, MacOSX\n\n\
    Philip Lange\n\
      OpenCPN Documentation\n\n\
    Ron Kuris\n\
      wxWidgets Support\n\n\
    Julian Smart, Robert Roebling et al\n\
      wxWidgets Authors\n\n\
    Sylvain Duclos\n\
      S52 Presentation Library code\n\n\
    Manish P. Pagey\n\
      Serial Port Library\n\n\
    David Flater\n\
      XTIDE tide and current code\n\n\
    Frank Warmerdam\n\
      GDAL Class Library\n\n\
    Mike Higgins\n\
      BSB Chart Format Detail\n\n\
    Samuel R. Blackburn\n\
      NMEA0183 Class Library\n\n\
    Atul Narkhede\n\
      Polygon Graphics utilities\n\n\
    Jan C. Depner\n\
      WVS Chart Library\n\n\
    Stuart Cunningham, et al\n\
      BSB Chart Georeferencing Algorithms\n\n\
    John F. Waers\n\
      UTM Conversion Algorithms\n\n\
    Carsten Tschach\n\
      UTM Conversion Algorithms\n\n\
    Ed Williams\n\
      Great Circle Formulary\n\n\
    Philippe Bekaert\n\
      CIE->RGB Color Conversion Matrix\n\n\
    Robert Lipe\n\
      Garmin USB GPS Interface\n"

};





IMPLEMENT_DYNAMIC_CLASS( about, wxDialog )


BEGIN_EVENT_TABLE( about, wxDialog )
    EVT_BUTTON( xID_OK, about::OnXidOkClick )
    EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK_HELP, about::OnPageChange)
    EVT_BUTTON( ID_DONATE, about::OnDonateClick)

END_EVENT_TABLE()


about::about( )
{
}

about::about( wxWindow* parent,wxString *pData_Locn, wxWindowID id, const wxString& caption,
                  const wxPoint& pos, const wxSize& size, long style)
{
  m_pDataLocn = pData_Locn;
#ifdef __WXOSX__
  style |= wxSTAY_ON_TOP;
#endif
  Create(parent, id, caption, pos, size, style);
  m_parent = parent;
}


bool about::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                     const wxPoint& pos, const wxSize& size, long style)
{
    SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    m_parent = parent;

    m_btips_loaded = false;


    CreateControls();
    Update();

    GetSizer()->Fit(this);
    GetSizer()->SetSizeHints(this);
    Centre();

    return TRUE;
}

void about::Update()
{
      pAboutTextCtl->Clear();
      wxString *pAboutString = new wxString(AboutText,  wxConvUTF8);

      pAboutString->Append(OpenCPNVersion); //Gunther
      pAboutString->Append(wxString(OpenCPNInfo,  wxConvUTF8));

      pAboutTextCtl->WriteText(*pAboutString);
      delete pAboutString;

  // Show the user where the log file is going to be
      wxString log = _T("    Logfile location: ");
      log.Append(glog_file);
      pAboutTextCtl->WriteText(log);

 // Show the user where the config file is going to be
      wxString conf = _T("\n    Config file location: ");
      conf.Append(gConfig_File);
      pAboutTextCtl->WriteText(conf);

      pAuthorTextCtl->Clear();
      wxString *pAuthorsString = new wxString(AuthorText,  wxConvUTF8);
      pAuthorTextCtl->WriteText(*pAuthorsString);
      delete pAuthorsString;


      pLicenseTextCtl->Clear();
      wxString license_loc(*m_pDataLocn);
      license_loc.Append(_T("license.txt"));

      wxTextFile license_file(license_loc);

      if(license_file.Open())
      {
            wxString str;
            str = license_file.GetFirstLine();
            pLicenseTextCtl->WriteText(str);

            while (!license_file.Eof())
            {
                  str = license_file.GetNextLine();
                  str.Append(_T("\n"));
                  pLicenseTextCtl->AppendText(str);
            }
            license_file.Close();
      }
      else
      {
            wxString msg(_T("Could not open License file: "));
            msg.Append(license_loc);
            wxLogMessage(msg);
      }
      pLicenseTextCtl->SetInsertionPoint(0);

      DimeControl(this);
}

void about::CreateControls()
{

  about* itemDialog1 = this;

  wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
  itemDialog1->SetSizer(itemBoxSizer2);


  wxStaticText *pST1 = new wxStaticText(this, -1, _T("Label"), wxDefaultPosition, wxSize(500, 50));
  pST1->SetLabel(_("\n         OpenCPN...A Nice Little Open Source Chart Plotter/Navigator"));
  pST1->InheritAttributes();
  itemBoxSizer2->Add(pST1);

  //   "Donate" Button
  ocpnStyle::Style* style = g_StyleManager->GetCurrentStyle();
  wxBitmap donate_bmp = style->GetIcon( _T("donate") );

  wxButton* donateButton = new wxBitmapButton( itemDialog1, ID_DONATE, donate_bmp, wxDefaultPosition, wxDefaultSize, 0 );
  donateButton->SetDefault();
  itemBoxSizer2->Add(donateButton, 0, wxALIGN_RIGHT|wxRIGHT, 5);

  //  Main Notebook
  wxNotebook* itemNotebook4 = new wxNotebook( itemDialog1, ID_NOTEBOOK_HELP, wxDefaultPosition, wxSize(-1, -1), wxNB_TOP );
  itemNotebook4->InheritAttributes();
  itemBoxSizer2->Add(itemNotebook4, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxALL, 5);

  //    About Panel
  itemPanelAbout = new wxPanel( itemNotebook4, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
  itemPanelAbout->InheritAttributes();
  itemNotebook4->AddPage(itemPanelAbout, _("About"));

  wxBoxSizer* itemBoxSizer6 = new wxBoxSizer(wxVERTICAL);
  itemPanelAbout->SetSizer(itemBoxSizer6);

  pAboutTextCtl = new wxTextCtrl( itemPanelAbout, -1, _T(""), wxDefaultPosition, wxSize(-1, 300),
                                              wxTE_MULTILINE | wxTE_READONLY );
  pAboutTextCtl->InheritAttributes();
  itemBoxSizer6->Add(pAboutTextCtl, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxALL, 5);



  //     Authors Panel
  itemPanelAuthors = new wxPanel( itemNotebook4, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
  itemPanelAuthors->InheritAttributes();
  itemNotebook4->AddPage(itemPanelAuthors, _("Authors"));

  wxBoxSizer* itemBoxSizer7 = new wxBoxSizer(wxVERTICAL);
  itemPanelAuthors->SetSizer(itemBoxSizer7);

  pAuthorTextCtl = new wxTextCtrl( itemPanelAuthors, -1, _T(""), wxDefaultPosition, wxSize(-1, 300),
                                               wxTE_MULTILINE | wxTE_READONLY );
  pAuthorTextCtl->InheritAttributes();
  itemBoxSizer7->Add(pAuthorTextCtl, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxALL, 5);



  //  License Panel
  itemPanelLicense = new wxPanel( itemNotebook4, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
  itemPanelLicense->InheritAttributes();
  itemNotebook4->AddPage(itemPanelLicense, _("License"));

  wxBoxSizer* itemBoxSizer8 = new wxBoxSizer(wxVERTICAL);
  itemPanelLicense->SetSizer(itemBoxSizer8);


  int tcflags = wxTE_MULTILINE | wxTE_READONLY;

  //    wxX11 TextCtrl is broken in many ways.
  //    Here, the wxTE_DONTWRAP flag creates a horizontal scroll bar
  //    which fails in wxX11 2.8.2....
#ifndef __WXX11__
  tcflags |= wxTE_DONTWRAP;
#endif
  pLicenseTextCtl = new wxTextCtrl( itemPanelLicense, -1, _T(""), wxDefaultPosition, wxSize(-1, 300), tcflags);

  pLicenseTextCtl->InheritAttributes();
  itemBoxSizer8->Add(pLicenseTextCtl, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxALL, 5);

    //     Tips Panel
  itemPanelTips = new wxPanel( itemNotebook4, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
  itemPanelTips->InheritAttributes();
  itemNotebook4->AddPage(itemPanelTips, _("Help"));

  wxBoxSizer* itemBoxSizer9 = new wxBoxSizer(wxVERTICAL);
  itemPanelTips->SetSizer(itemBoxSizer9);


//  m_ptips_window = new  wxHtmlWindow(itemPanelTips, -1, wxDefaultPosition, wxSize(100,400), wxHW_DEFAULT_STYLE, _T("OpenCPN Help"));
//  itemBoxSizer9->Add(m_ptips_window, 0, wxGROW);


  //    Close Button

  wxBoxSizer* itemBoxSizer28 = new wxBoxSizer(wxHORIZONTAL);
  itemBoxSizer2->Add(itemBoxSizer28, 0, wxALIGN_RIGHT|wxALL, 5);

  wxButton* itemButton29 = new wxButton( itemDialog1, xID_OK, _("Close"), wxDefaultPosition, wxDefaultSize, 0 );
  itemButton29->SetDefault();
  itemButton29->InheritAttributes();
  itemBoxSizer28->Add(itemButton29, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
}


void about::OnXidOkClick( wxCommandEvent& event )
{
  Close();
}

void about::OnDonateClick( wxCommandEvent& event )
{
      wxLaunchDefaultBrowser(_T("https://sourceforge.net/donate/index.php?group_id=180842"));
}

void about::OnPageChange(wxNotebookEvent& event)
{
      int i = event.GetSelection();

      if(3 == i)                        // 3 is the index of "Help" page
      {
            wxString def_lang_canonical = wxLocale::GetLanguageInfo(wxLANGUAGE_DEFAULT)->CanonicalName;

            wxString help_locn = _T("doc/help_");
            help_locn.Prepend(*m_pDataLocn);

            wxString help_try = help_locn;
            help_try += def_lang_canonical;
            help_try += _T(".html");

            if(::wxFileExists(help_try))
                  wxLaunchDefaultBrowser(wxString(_T("file:///")) + help_try);

            else
            {
                  help_try = help_locn;
                  help_try += _T("en_US");
                  help_try += _T(".html");

                  if(::wxFileExists(help_try))
                        wxLaunchDefaultBrowser(wxString(_T("file:///")) + help_try);
                  else
                  {
                        help_try = _T("doc/help_web.html");
                        help_try.Prepend(*m_pDataLocn);
                        if(::wxFileExists(help_try))
                              wxLaunchDefaultBrowser(wxString(_T("file:///")) + help_try);
                  }
            }
      }
}


