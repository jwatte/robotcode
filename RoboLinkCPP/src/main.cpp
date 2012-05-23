
#include <stdio.h>
#include <wx/utils.h>
#include <wx/wx.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/menuitem.h>

enum
{
  CMD_Quit = 1,
  CMD_About = 2
};

class RoboLinkFrame : public wxFrame
{
public:
  RoboLinkFrame(wxString const &title, wxPoint const &pos, wxSize const &size)
  {
    wxMenu *menu = new wxMenu();
    menu->Append(CMD_About, _("&About..."));
    menu->AppendSeparator();
    menu->Append(CMD_Quit, _("&Quit..."));
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(menu, _("&File"));
    SetMenuBar(menuBar);
    CreateStatusBar();
    SetStatusText(_("Not Yet Connected"));
  }
  void OnQuit(wxCommandEvent &evt)
  {
    Close(true);
  }
  void OnAbout(wxCommandEvent &evt)
  {
    wxMessageBox(
      _("Built " __DATE__ " " __TIME__),
      _("About RoboLinkCPP"),
      wxOK | wxICON_INFORMATION,
      this);
  }

  DECLARE_EVENT_TABLE()
};

class RoboLinkApp : public wxApp
{
public:
  virtual bool OnInit()
  {
    RoboLinkFrame *frame = new RoboLinkFrame(_("RoboLinkCPP"), wxPoint(5, 20), wxSize(1024, 640));
    frame->Show(true);
    SetTopWindow(frame);
    return true;
  }
};

BEGIN_EVENT_TABLE(RoboLinkFrame, wxFrame)
  EVT_MENU(CMD_Quit, RoboLinkFrame::OnQuit)
  EVT_MENU(CMD_About, RoboLinkFrame::OnAbout)
END_EVENT_TABLE()

IMPLEMENT_APP(RoboLinkApp)



