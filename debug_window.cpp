#include "stdafx.h"
#include "main.h"
#include "debug.h"
#include "resources.h"
#include "plugins.h"
#include "tables.h"
#include "imessage.h"
#include "connections.h"
#include "threads.h"
#include "argv.h"

using namespace Stamina;

namespace Konnekt { namespace Debug {

#ifdef __DEBUG
	bool IMfinished = true;
	bool debug=true;

	int x;
	int y;
	int w;
	int h;
	bool show;
	bool log;
	bool scroll;
	bool logAll;
	bool test = false;
	bool showLog = false;
	HWND hwnd = 0 , hwndStatus , hwndLog , hwndBar, hwndCommand;
	HIMAGELIST iml;
	CriticalSection windowCSection;

	HINSTANCE instRE;

	void testStart();
	void testEnd();
	LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
	unsigned int __stdcall debugThreadProc(void * param);

	// ----------------------------------------------------------

	void startup(HINSTANCE hInst) {
		if (!superUser) return;
		instRE = LoadLibrary("Riched20.dll");

		_beginthreadex(0, 0, debugThreadProc, 0, 0, 0);

		while (!Debug::hwnd) {
			Sleep(0);
		}

		if (argVExists(ARGV_STARTDEV)) {
			Debug::show = true;
			Debug::log = true;
		}

		return;
	}

	void finish() {
		DestroyWindow(Debug::hwnd);
	}


	//char sDebugInfo [MAX_LOADSTRING];
	//char sDebugNew [MAX_LOADSTRING];

	//char sVersionInfo [MAX_LOADSTRING];

	void ShowDebugWnds() {
		if (!superUser) return;
		if (Debug::show) ShowWindow(Debug::hwnd , SW_SHOW);
	}

	void onSizeDebug(int w , int h) {
		SendMessage(Debug::hwndStatus , WM_SIZE , 0,MAKELPARAM(w,h));
		HDWP hDwp;
		RECT rc , rc2;
		GetClientRect(Debug::hwnd , &rc);
		GetClientRect(Debug::hwndBar , &rc2);
		int barHeight = rc2.bottom - rc.top;
		rc.top += rc2.bottom + 2;
		rc.left+=2;
		rc.right-=2;
		GetClientRect(Debug::hwndStatus , &rc2);
		rc.bottom -= rc2.bottom + 2;

		w = rc.right - rc.left;
		h = rc.bottom - rc.top;

		const int commandHeight = 20;
		const int okWidth = 50;
		h -= commandHeight;
		HWND okItem = GetDlgItem(hwnd, IDOK);

		hDwp=BeginDeferWindowPos(2);
		hDwp=DeferWindowPos(hDwp , Debug::hwndBar ,0,
			2,2, w ,barHeight,SWP_NOZORDER);
		hDwp=DeferWindowPos(hDwp , Debug::hwndLog ,0,
			rc.left,rc.top, w ,h,SWP_NOZORDER);
		hDwp=DeferWindowPos(hDwp , Debug::hwndCommand ,0,
			rc.left + okWidth,h+rc.top, w - okWidth ,150,SWP_NOZORDER);
		hDwp=DeferWindowPos(hDwp , okItem ,0,
			rc.left,h+rc.top, okWidth ,commandHeight,SWP_NOZORDER);
		EndDeferWindowPos(hDwp);
	}


	void createWindows() {
#define WS_EX_COMPOSITED        0x02000000L
		if (!Debug::w) {
			Debug::x = Debug::y = 50;
			Debug::w = 400;
			Debug::h = 250;
			Debug::scroll=true;
		}
		Debug::hwnd = CreateDialog(Stamina::getHInstance() , MAKEINTRESOURCE(IDD_DEBUG) , 0 , (DLGPROC)WndProc);
		Debug::hwndLog = GetDlgItem(Debug::hwnd , IDC_MSG);
		Debug::hwndCommand = GetDlgItem(Debug::hwnd , IDC_COMMAND);
		SetWindowPos(Debug::hwnd , 0 , Debug::x , Debug::y , Debug::w , Debug::h , SWP_NOACTIVATE | SWP_NOZORDER);
		SendMessage(Debug::hwnd , WM_SETICON , ICON_SMALL , (LPARAM)loadIconEx(Stamina::getHInstance() , MAKEINTRESOURCE(IDI_DEBUG_ICON) , 16));
		SendMessage(Debug::hwnd , WM_SETICON , ICON_BIG , (LPARAM)loadIconEx(Stamina::getHInstance() , MAKEINTRESOURCE(IDI_DEBUG_ICON) , 32));

		Debug::hwndStatus = CreateStatusWindow(SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE,
			("Konnekt "+suiteVersionInfo).c_str(),Debug::hwnd,IDC_STATUSBAR);
		int sbw [3]={200 , -1};
		SendMessage(Debug::hwndStatus , SB_SETPARTS , 2 , (LPARAM)sbw);

		Debug::iml = ImageList_Create(16 , 16 , ILC_COLOR32|ILC_MASK	 , 5 , 5);
		//   icon = LoadIconEx(hInst , MAKEINTRESOURCE(IDI_HISTB_REFRESH) , 16);
		//   ImageList_AddIcon(Debug::iml , icon);
		//   DestroyIcon(icon);


		Debug::hwndBar = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR) NULL,
			WS_CHILD | WS_CLIPCHILDREN |WS_CLIPSIBLINGS | WS_VISIBLE
			|TBSTYLE_TRANSPARENT
			|CCS_NODIVIDER
			| TBSTYLE_FLAT
			| TBSTYLE_LIST
			//        | CCS_NOPARENTALIGN
			//        | CCS_NORESIZE
			| TBSTYLE_TOOLTIPS
			//| CCS_NOPARENTALIGN
			| CCS_NORESIZE

			, 0, 0, 200, 30, Debug::hwnd,
			(HMENU)IDC_TOOLBAR, Stamina::getHInstance(), 0);
		// Get the height of the toolbar.
		SendMessage(Debug::hwndBar , TB_SETEXTENDEDSTYLE , 0 ,
			TBSTYLE_EX_MIXEDBUTTONS
			| TBSTYLE_EX_HIDECLIPPEDBUTTONS
			);
		SendMessage(Debug::hwndBar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
		SendMessage(Debug::hwndBar, TB_SETIMAGELIST, 0, (LPARAM)Debug::iml);
		// Set values unique to the band with the toolbar.
		TBBUTTON bb;
		bb.dwData=0;
#define INSERT(cmd , str , bmp , state , style)\
	bb.iString=(int)(str);\
	bb.idCommand=(cmd);\
	bb.iBitmap=(bmp);\
	bb.fsState=(state)| TBSTATE_ENABLED;\
	bb.fsStyle=(style)| BTNS_AUTOSIZE | (bmp==I_IMAGENONE?BTNS_SHOWTEXT:0);\
	SendMessage(Debug::hwndBar, TB_INSERTBUTTON, 0x3FFF , (LPARAM)&bb)
		INSERT(IDB_SHUT , "Zako�cz" , I_IMAGENONE , 0 , BTNS_CHECK);
		INSERT(0,"",0,0,BTNS_SEP);
		INSERT(IDB_LOG , "Loguj" , I_IMAGENONE , Debug::log?TBSTATE_CHECKED:0 , BTNS_CHECK);
		if (Debug::debugAll) {
			INSERT(IDB_LOGALL , "Wszystko" , I_IMAGENONE , Debug::logAll?TBSTATE_CHECKED:0 , BTNS_CHECK);
		}
		INSERT(IDB_LOGCLEAR , "Wyczy��" , I_IMAGENONE , 0 , BTNS_BUTTON);
		INSERT(IDB_MARK , "Zaznacz" , I_IMAGENONE , 0 , BTNS_BUTTON);
		INSERT(0,"",0,0,BTNS_SEP);
		INSERT(IDB_SDK , "SDK" , I_IMAGENONE , 0 , BTNS_BUTTON);
		INSERT(IDB_INFO , "Info" , I_IMAGENONE , 0 , BTNS_BUTTON);
		INSERT(IDB_QUEUE , "Kolejka" , I_IMAGENONE , 0 , BTNS_BUTTON);
		INSERT(IDB_SCROLL , "Przewijaj" , I_IMAGENONE , Debug::scroll?TBSTATE_CHECKED:0 , BTNS_CHECK);
		INSERT(0,"",0,0,BTNS_SEP);
		INSERT(IDB_TEST , "Test" , I_IMAGENONE , 0 , BTNS_CHECK);
		INSERT(IDB_UI , "UI" , I_IMAGENONE , 0 , BTNS_BUTTON);
		//     INSERT(IDB_MSG , "Loguj" , 1 , Debug::log?TBSTATE_CHECKED:0 , BTNS_CHECK);
#undef INSERT
		debugLog();
		onSizeDebug(0,0);
	}

	unsigned int __stdcall debugThreadProc(void * param) {
		// tworzy okno i przyleg�o�ci...
		Debug::threadId = GetCurrentThreadId();
		Debug::createWindows();
		MSG msg;
		int bRet;
		while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
		{ 
			if (bRet == -1)
			{
				return 0;// handle the error and possibly exit
			}
			else
			{
				TranslateMessage(&msg); 
				DispatchMessage(&msg); 
			}
		}
		return 0;
	}


	void runDebugCommand() {
		tStringVector list;
		CStdString command;
		GetWindowText(Debug::hwndCommand, command.GetBuffer(1024), 1024);
		command.ReleaseBuffer();
		splitCommand(command, ' ', list);
		const char ** argv = new const char * [list.size()];
		for (unsigned int i = 0; i < list.size(); i++) {
			argv[i] = list[i].c_str();
		}
		sIMessage_debugCommand pa(list.size(), argv, sIMessage_debugCommand::asynchronous);
		IMessage(&pa);
		delete [] argv;
	}


	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		//if (hWnd == hwndDebugGSend) MessageBox(0,"ee","",0);
		static bool quitOnce = false;
		string str1,str2;
		switch (message) {
		case WM_CLOSE:
			ShowWindow(hWnd , SW_HIDE);
			break;
		case WM_DESTROY:
			break;
		case WM_SIZING:
			clipSize(wParam , (RECT *)lParam , 250 , 100);
			return 1;
		case WM_SIZE:
			onSizeDebug(LOWORD(lParam) , HIWORD(lParam));
			return 1;
		case WM_COMMAND:
			if (HIWORD(wParam)==BN_CLICKED)	{
				switch (LOWORD(wParam)) {
				case IDB_SHUT:
					//SendMessage(hWnd , WM_ENDSESSION , 0 , 0);break;
					if (quitOnce)
						exit(0);
					else {
						SendMessage(Debug::hwndBar , TB_CHECKBUTTON , IDB_SHUT , 1);
						debugLogMsg("Je�li naci�niesz [zamknij] jeszcze raz, program zostanie natychmiast przerwany!");
						PostQuitMessage(0);
					}
					quitOnce = true;
					break;
				case IDB_SDK:
					ShellExecute(0 , "open" , loadString(IDS_DEBUG_URL_SDK).c_str() , "" , "" , SW_SHOW);
					break;
				case IDB_INFO:
					debugLogInfo();
					break;
				case IDB_QUEUE:
					debugLogQueue();
					break;
				case IDB_LOG:
					Debug::log = !Debug::log;
					debugLogMsg(string("Logowanie ")+(Debug::log?"w��czone":"wy��czone"));
					SendMessage(Debug::hwndBar , TB_CHECKBUTTON , IDB_LOG , Debug::log);                                
					break;
				case IDB_LOGALL:
					Debug::logAll = !Debug::logAll;
					debugLogMsg(string("Pokazuje ")+(Debug::logAll?"wszystko":"tylko IMC_LOG"));
					SendMessage(Debug::hwndBar , TB_CHECKBUTTON , IDB_LOGALL , Debug::logAll);                                
					break;
				case IDB_SCROLL:
					Debug::scroll = !Debug::scroll;
					SendMessage(Debug::hwndBar , TB_CHECKBUTTON , IDB_LOG , Debug::log);
					break;
				case IDB_TEST:
					Debug::test = !Debug::test;
					SendMessage(Debug::hwndBar , TB_CHECKBUTTON , IDB_TEST , Debug::test);
					if (Debug::test) Debug::testStart();
					else Debug::testEnd();
					break;
				case IDB_LOGCLEAR:
					SetWindowText(Debug::hwndLog , "");
					break;
				case IDB_MARK:
					if (Debug::logFile) {
						static int mark = 0;
						fprintf(Debug::logFile , "\n\n~~~~~~~~~~~~~~~~~~~~ %d ~~~~~~~~~~~~~~~~~~~~~\n\n",++mark);
						fflush(Debug::logFile);
						debugLogMsg(stringf("Zaznaczenie wstawione do "+logFileName+" pod numerem %d",mark));
					}
					break;
				case IDB_UI: plugins[pluginUI].IMessageDirect(IMI_DEBUG, 0, 0);    
					break;
				case IDOK:
					runDebugCommand();
					break;

				}
			}
			break;

			//		default:
			//			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	}


	// ---------------------------




#define RE_()   HWND hwnd = Debug::hwndLog;CHARFORMAT2 cf;PARAFORMAT pf;pf.cbSize = sizeof(pf);cf.cbSize = sizeof(cf);
#define RE_PREPARE() {int ndx = GetWindowTextLength (hwnd); SendMessage(hwnd , EM_SETSEL ,  ndx, ndx);}
#define RE_ALIGNMENT(al) pf.dwMask = PFM_ALIGNMENT;pf.wAlignment = al;SendMessage(hwnd , EM_SETPARAFORMAT , 0 , (LPARAM)&pf)
#define RE_FACE(fac) cf.dwMask = CFM_FACE;strcpy(cf.szFaceName , fac);SendMessage(hwnd , EM_SETCHARFORMAT , SCF_SELECTION , (LPARAM)&cf)
#define RE_SIZE(siz) cf.dwMask = CFM_SIZE;cf.yHeight = (siz);SendMessage(hwnd , EM_SETCHARFORMAT , SCF_SELECTION , (LPARAM)&cf)
#define RE_BOLD(bo) cf.dwMask = CFM_BOLD;cf.dwEffects = bo?CFM_BOLD:0;SendMessage(hwnd , EM_SETCHARFORMAT , SCF_SELECTION , (LPARAM)&cf)
#define RE_COLOR(co) cf.dwMask = CFM_COLOR;cf.crTextColor = co;SendMessage(hwnd , EM_SETCHARFORMAT , SCF_SELECTION , (LPARAM)&cf)
#define RE_BGCOLOR(co) cf.dwMask = CFM_BACKCOLOR;cf.crBackColor = co;SendMessage(hwnd , EM_SETCHARFORMAT , SCF_SELECTION , (LPARAM)&cf)
#define RE_ADD(txt) SendMessage(hwnd , EM_REPLACESEL , 0 , (LPARAM)(string(txt).c_str()))
#define RE_ADDLINE(txt) RE_ADD(string(txt) + "\r\n")
#define RE_SCROLLDOWN() {SendMessage(hwnd , WM_VSCROLL , SB_BOTTOM , 0);\
	if (SendMessage(hwnd , EM_GETFIRSTVISIBLELINE , 0 , 0) == SendMessage(hwnd , EM_GETLINECOUNT , 0 , 0)-1) SendMessage(hwnd , EM_SCROLL , SB_PAGEUP , 0);}
	void debugLogInfo(){
		RE_();
		RE_PREPARE();
		RE_COLOR(RGB(0,0,0x80));
		RE_BOLD(1);
		RE_ADD("\r\n\r\n------------ INFO ------------\r\n");
		RE_COLOR(RGB(0xFF,0,0));
		RE_ADD("WTYCZKI:");
		RE_COLOR(0);
		for (int i=0; i < plugins.count(); i++) {
			RE_BOLD(1);
			RE_ADD("\r\n -> " + plugins[i].getDllFile());  
			RE_BOLD(0);
			RE_ADD(stringf("\r\n    ID = %d hModule = 0x%x net = %d type = %x prrt = %d " , plugins[i].getId() , plugins[i].getDllModule() , plugins[i].getNet() , plugins[i].getType() ,  plugins[i].getPriority()));
			RE_ADD(stringf("\r\n    name = \"%s\" sig=\"%s\"  v %s" , plugins[i].getName().c_str() , plugins[i].getSig().c_str() , plugins[i].getVersion().getString().c_str()));
		}

		RE_BOLD(1);
		RE_COLOR(RGB(0xFF,0,0));
		RE_ADD("\r\nCONN:");
		RE_COLOR(0);
		RE_BOLD(0);
		for (Connections::tList::iterator item = Connections::getList().begin(); item != Connections::getList().end(); item++) {
			RE_ADD(stringf("\r\n -> %s %s %d retries" , plugins.getName((tPluginId)item->first).c_str() , item->second.connect?"Awaiting connection":"Idle" , item->second.retry));
		}

		RE_ADD(".\r\n");
	}

	void debugLogQueue(){
		RE_();
		RE_PREPARE();
		Tables::oTableImpl msg(Tables::tableMessages);
		msg->lockData(DT::allRows);
		for (unsigned int i=0; i<msg->getRowCount(); i++) {
			RE_BOLD(1);
			RE_COLOR(RGB(0x0,80,0));
			RE_ADD(stringf("\r\nMSG %x %s from '%s' to '%s' [%s]\r\n", msg->getInt(i,Message::colId) 
				, IMessage(IM_PLUG_NETNAME, (tNet)msg->getInt(i,Message::colNet),IMT_PROTOCOL)
				, msg->getString(i,Message::colFromUid).c_str()
				, msg->getString(i,Message::colToUid).c_str()
				, msg->getString(i,Message::colBody).substr(0,30).c_str()));
			RE_BOLD(0);
			RE_COLOR(RGB(0,0,0));
			RE_ADD("EXT []" + msg->getString(i , Message::colExt) + "\r\n");
			int flag = msg->getInt(i,Message::colFlag);
      RE_ADD(stringf("Type=%d  flag=%x " , msg->getInt(i,Message::colType)
				, flag));
			RE_COLOR(RGB(80,0,0));
			RE_ADD(stringf("%s %s %s\r\n" 
				, flag&Message::flagSend?"MF_SEND":""
				, flag&Message::flagProcessing?"MF_PROCESSING":""
				, flag&Message::flagOpened?"MF_OPENED":""
				));
			RE_COLOR(RGB(0,0,0));
		}
		msg->unlockData(DT::allRows);
		RE_ADD(".\r\n");
	}


	void debugLogPut(string msg) {
		RE_();RE_PREPARE();
		RE_ADD(msg);
	}
	void debugLogMsg(string msg) {
		RE_();RE_PREPARE();
		RE_COLOR(RGB(255,0,0));
		RE_BOLD(1);
		RE_ADD("\r\n\t"+msg+"\r\n\r\n");
		if (Debug::scroll) RE_SCROLLDOWN();
	}
	void debugStatus(int part , string msg) {
		SendMessage(Debug::hwndStatus , SB_SETTEXT , part|SBT_NOBORDERS , (LPARAM)msg.c_str());
	}
	void debugLogValue(string name , string value){

	}

	void debugLog() {
		RE_();
		RE_PREPARE();
		RE_BOLD(1);
		RE_ADD("Konnekt@Dev ");
		RE_BOLD(0);
		RE_ADDLINE(suiteVersionInfo);
		RE_COLOR(RGB(0,0,128));
		string info = stringf("ComCtl6_present = %d (%s message loop)\r\n"
#ifdef __DEBUG
			"Wersja DEBUG - logowanie w��czone!\r\n"
#endif
#ifdef __WITHDEBUGALL
			"Wersja VERBOSE - mo�liwo�� logowania wszystkich wiadomo�ci!\r\n"
#endif
			"", isComctl(6,0) , isComctl(6,0) ? "szybszy":"wolniejszy");
		if (Debug::debugAll) {
			info += "logowanie wszystkich wiadomo�ci w��czone\r\n";
		}
		if (Konnekt::noCatch) {
			info += "logowanie wszystkich wiadomo�ci w��czone\r\n";
		}
		RE_ADDLINE(info);
		/*		RE_COLOR(RGB(255,0,0));
		RE_BOLD(1);
		RE_ADDLINE("Logowanie do tego okna mo�e zwiesi� program w sytuacji gdy dwa w�tki jednocze�nie b�d� logowa�y! Pracuj� nad naprawieniem tej sytuacji...\r\n");
		RE_BOLD(0);*/
		RE_COLOR(RGB(0,0,0));
	}
#define COLOR_SENDER RGB(30,30,30)
#define COLOR_RCVR   RGB(60,60,60)
#define COLOR_LOG    RGB(0,50,0)
#define COLOR_MODULE    RGB(0,0,150)
#define COLOR_WHERE    RGB(0,0,200)
#define COLOR_IM     RGB(80,80,80)
#define COLOR_ID     RGB(00,00,80)
#define COLOR_P      RGB(20,20,20)
#define COLOR_RES    RGB(00,80,00)
#define COLOR_ERR    RGB(80,00,00)
#define COLOR_THREAD RGB(100,00,80)
#define COLOR_NR     RGB(200,200,200)
#define COLOR_BC     RGB(00,0xD0,00)

	void debugLogMsg(Plugin& plugin, LogLevel level, const char* module, const char* where, const StringRef& msg) {

		if ((level & DBG_SPECIAL) == 0 && (!superUser || !Debug::log || !Debug::showLog)) return;
		Locker lock(windowCSection);
		RE_();
		RE_PREPARE();
		RE_BOLD(0);
		RE_COLOR(0);
		RE_ADD("\r\n");
		RE_BGCOLOR(plugin.getDebugColor());
		RE_ADD("  ");
		RE_BGCOLOR(TLSU().color);
		if (Debug::logAll) {
			RE_ADD(Debug::logIndent());
		}
		RE_COLOR(COLOR_LOG);
		RE_BOLD(1);
		RE_ADD("## ");
		RE_ADD(plugin.getName());
		if ((module && *module) || (where && *where)) {
			RE_BOLD(0);
			RE_ADD(" \t ");
		}
		if ((module && *module)) {
			RE_COLOR(COLOR_MODULE);
			RE_ADD(module);
		}
		if ((where && *where)) {
			if ((module && *module)) {
				RE_ADD("::");
			}
			RE_COLOR(COLOR_MODULE);
			RE_ADD(where);
		}

		if (mainThread.isCurrent() == false) {
			RE_COLOR(COLOR_THREAD);
			RE_ADD( "/" + IMessageInfo::getThread() );
		}

		bool bold = false;
		switch (level) {
			case DBG_ERROR:
				bold = true;
				RE_COLOR(0x0000FF);
				break;
			case DBG_WARN:
				bold = true;
				RE_COLOR(RGB(0xFF, 0x99, 00));
				break;
			case DBG_TEST_TITLE:
				bold = true;
				RE_COLOR(0x800000);
				break;
			case DBG_TEST_PASSED:
				bold = true;
				RE_COLOR(0x00A000);
				break;
			case DBG_TEST_FAILED:
				bold = true;
				RE_COLOR(0x0000A0);
				break;
		}
		RE_BOLD(bold);
		RE_ADD(" \t " + msg);
		if (bold) RE_BOLD(1);
		RE_BGCOLOR(0xffffff);
		if (Debug::scroll) RE_SCROLLDOWN();

	}


	void debugLogIMStart(sIMessage_base * msg, Plugin& receiver) {
		if (!superUser || !Debug::log || !Debug::showLog || !Debug::logAll) return;
		Locker lock(windowCSection);
		RE_();
		RE_PREPARE();
		RE_BOLD(0);
		RE_COLOR(0);
		if (!IMfinished) RE_ADD("\r\n");
		RE_BGCOLOR(plugins[msg->sender].getDebugColor());
		RE_ADD("  ");
		RE_BGCOLOR(TLSU().color);
		RE_ADD(Debug::logIndent());
		
		IMessageInfo info(msg);

		RE_COLOR(COLOR_NR);RE_ADD( inttostr(imessageCount) +" ");
		RE_COLOR(COLOR_SENDER); RE_ADD(info.getSender() +" -");
		RE_COLOR(COLOR_RCVR); RE_ADD("> "+info.getPlugin(receiver));
		RE_COLOR(COLOR_IM);RE_ADD("\t | ");
		RE_COLOR(COLOR_ID);RE_BOLD(1);
		RE_ADD(info.getId());
		RE_BOLD(0);
		RE_COLOR(COLOR_IM);RE_ADD("\t | ");
		RE_COLOR(COLOR_P);
		RE_ADD(info.getData());
		RE_BGCOLOR(0xffffff);
		if (Debug::scroll) RE_SCROLLDOWN();
	}

	void debugLogIMEnd(sIMessage_base * msg , int result, bool multiline) {
		if (!superUser || !Debug::log || !Debug::showLog || !Debug::logAll) return;

		Locker lock(windowCSection);

		RE_();
		RE_PREPARE();
		if (multiline) {
			RE_ADD(Debug::logIndent());
		} else RE_ADD("\t");
		RE_BGCOLOR(TLSU().color);
		RE_ADD("= ");
		RE_COLOR(COLOR_RES);
		IMessageInfo info (msg);
		RE_ADD(info.getResult(result));
		if (TLSU().stack.getError()) {
			RE_COLOR(COLOR_ERR);RE_BOLD(1);
			RE_ADD("\t e"+IMessageInfo::getError(TLSU().stack.getError()));RE_BOLD(0);
		}
		RE_COLOR(COLOR_THREAD);
		RE_ADD("\t "+IMessageInfo::getThread());
		RE_ADD("\r\n");
		RE_BGCOLOR(0xffffff);
		if (Debug::scroll) RE_SCROLLDOWN();
	}



	void debugLogIMBCStart(sIMessage_base * msg) {
		if (!superUser || !Debug::log || !Debug::showLog || !Debug::logAll) return;
		Locker lock(windowCSection);
		RE_();
		RE_PREPARE();
		RE_BOLD(1);
		RE_COLOR(0);
		if (!IMfinished) RE_ADD("\r\n");
		RE_BGCOLOR(plugins[msg->sender].getDebugColor());
		RE_ADD("  ");
		RE_BGCOLOR(TLSU().color);
		RE_ADD(Debug::logIndent());
		
		IMessageInfo info(msg);

		RE_COLOR(COLOR_NR);RE_ADD( inttostr(imessageCount) +" ");
		RE_COLOR(COLOR_SENDER); RE_ADD(info.getSender());
		RE_COLOR(COLOR_BC); RE_ADD("  " + info.getBroadcast(msg->net)+" , " + info.getType());
		RE_COLOR(COLOR_IM);RE_ADD("\t | ");
		RE_COLOR(COLOR_ID);RE_BOLD(1);
		RE_ADD(info.getId());
		RE_BOLD(0);
		RE_COLOR(COLOR_IM);RE_ADD("\t | ");
		RE_COLOR(COLOR_P);
		RE_ADD(info.getData());
		RE_ADD(" : \r\n");
		IMfinished = true;
		RE_BGCOLOR(0xffffff);
		if (Debug::scroll) RE_SCROLLDOWN();
	}

	void debugLogIMBCEnd(sIMessage_base * msg , int result, int hit) {
		if (!superUser || !Debug::log || !Debug::showLog || !Debug::logAll) return;
		Locker lock(windowCSection);
		RE_();
		RE_PREPARE();
		if (!IMfinished) {
			RE_ADD("\r\n");
		}
		RE_ADD(Debug::logIndent());
		RE_BGCOLOR(TLSU().color);
		RE_BOLD(1);
		RE_COLOR(COLOR_BC);
		RE_ADD("=== ");
		RE_COLOR(COLOR_RES);

		IMessageInfo info(msg);

		RE_ADD(info.getResult(result));
		RE_COLOR(COLOR_BC);
		RE_ADD(" from " + inttostr(hit));
		RE_COLOR(COLOR_THREAD);
		RE_ADD("\t "+info.getThread());
		RE_ADD("\r\n");
		RE_BGCOLOR(0xffffff);
		if (Debug::scroll) RE_SCROLLDOWN();
	}




#undef  RE_PREPARE
#undef  RE_ALIGNMENT
#undef  RE_BOLD
#undef  RE_COLOR
#undef  RE_ADD
#undef  RE_ADDLINE

#define TIMEOUT_TEST 2000
	// ------------------------------
	HANDLE dbgEvent=0;
	VOID CALLBACK dbgTestAPC(ULONG_PTR param) {
		if (dbgEvent) SetEvent(dbgEvent);
	}

	void dbgTestThread(void * p) {
		dbgEvent = CreateEvent(0,0,0,0);
		while (Debug::test) {
			debugStatus(1,"APCqueued...");
			int time = GetTickCount();
			QueueUserAPC(dbgTestAPC,hMainThread,0);
			if (WaitForSingleObject(dbgEvent , TIMEOUT_TEST)==WAIT_TIMEOUT) {
				Beep(200,20);
				debugLogMsg(stringf("G��wny w�tek jest nieaktywny ponad %.1f s!",(float)(TIMEOUT_TEST/1000)));
				debugStatus(1,"G��wny w�tek nie odpowiada!");
			} else {debugStatus(1,stringf("APCtime: %.3f s" , (float)(GetTickCount()-time)/1000,"yo?"));}

			Sleep(2000);
		}
		dbgEvent=0;
		CloseHandle(dbgEvent);
	}

	void testStart() {
		_beginthread(dbgTestThread,0,0);
	}
	void testEnd() {

	}



#endif

};};