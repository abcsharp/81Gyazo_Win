// 81Gyazo.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "resource.h"

//グローバル変数
const wchar_t* Title=L"81Gyazo",
	*WindowClass=L"81GYAZOWIN",
	*WindowClassL=L"81GYAZOWINL",
	*TempPrefix=L"81gya",
	*UploadServer=L"g.81.la",
	*ScriptName=L"upload.cgi";
HWND LayerWindowHandle;
int OffsetX, OffsetY;//画面オフセット
Gdiplus::GdiplusStartupInput StartupInput;
ULONG_PTR Token;
std::shared_ptr<void> Brush;//HBRUSH

//プロトタイプ宣言
void ShutdownGdiplus(void);
void RegisterWindowClass(HINSTANCE Instance);
bool InitInstance(HINSTANCE Instance);
bool GetEncoderClassID(const std::wstring& MimeType,CLSID* ClassID);
bool IsPNG(const std::wstring& FileName);
bool ConvertPNG(const std::wstring& DestFile,const std::wstring& SrcFile);
bool SavePNG(const std::wstring& FileName,HBITMAP NewBitmap);
void DrawRubberband(RECT& NewRect);
LRESULT __stdcall LayerWndProc(HWND WindowHandle,UINT Message,WPARAM WParam,LPARAM LParam);
LRESULT __stdcall WndProc(HWND WindowHandle,UINT Message,WPARAM WParam,LPARAM LParam);
void SetClipBoardText(const std::wstring& String);
void OpenUrl(const std::wstring& Url);
std::string GetID();
bool SaveID(const std::wstring& IDString);
bool UploadFile(const std::wstring& FileName);
std::wstring ToUnicode(const std::string& MultiByteString);
std::wstring ToUnicode(const std::vector<char>& MultiByteBuffer);
std::string ToMultiByte(const std::wstring& UnicodeString);
std::string ToMultiByte(const std::vector<wchar_t>& UnicodeBuffer);

//エントリーポイント
int __stdcall wWinMain(HINSTANCE Instance,HINSTANCE,LPWSTR,int)
{
	MSG Msg;

	std::vector<wchar_t> Buffer(MAX_PATH,0);
	DWORD Length;

	//自身のディレクトリを取得する
	Length=GetModuleFileName(nullptr,Buffer.data(),MAX_PATH);
	std::wstring ThisPath(Buffer.data());
	ThisPath[ThisPath.find_last_of(L'\\')]=L'\0';

	//カレントディレクトリをexeと同じ場所に設定
	SetCurrentDirectory(ThisPath.c_str());

	if(Gdiplus::GdiplusStartup(&Token,&StartupInput,nullptr)!=Gdiplus::Status::Ok) return 1;

	Brush.reset(CreateSolidBrush(RGB(100,100,100)),[](void* Obj){if(Obj) DeleteObject(Obj);});

	std::atexit(ShutdownGdiplus);
	//引数にファイルが指定されていたら
	if(__argc==2){
		//ファイルをアップロードして終了
		if(IsPNG(__wargv[1])){
			//PNG はそのままアップロード
			UploadFile(__wargv[1]);
		}else{
			//PNG 形式に変換
			std::vector<wchar_t> TempDirectory(MAX_PATH,0),TempFile(MAX_PATH,0);
			GetTempPath(MAX_PATH,TempDirectory.data());
			GetTempFileName(TempDirectory.data(),TempPrefix,0,TempFile.data());
			if(ConvertPNG(TempFile.data(),__wargv[1])){
				//アップロード
				UploadFile(TempFile.data());
			}else{
				//PNGに変換できなかった...
				MessageBox(nullptr,L"画像をPNG形式に変換する事が出来ません。",Title,MB_OK|MB_ICONERROR);
			}
			DeleteFile(TempFile.data());
		}
		return true;
	}

	//ウィンドウクラスを登録
	RegisterWindowClass(Instance);

	//アプリケーションの初期化
	if(!InitInstance(Instance)) return 1;
	
	//メインメッセージループ
	while(GetMessage(&Msg,nullptr,0,0)){
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return (int)Msg.wParam;
}

void ShutdownGdiplus(void)
{
	Gdiplus::GdiplusShutdown(Token);
	return;
}

//ウィンドウクラスを登録
void RegisterWindowClass(HINSTANCE Instance)
{
	WNDCLASSEX WndClass;
	ZeroMemory(&WndClass,sizeof(WndClass));
	WndClass.cbSize=sizeof(WndClass);
	//メインウィンドウ
	WndClass.style=0;//WM_PAINTを送らない
	WndClass.lpfnWndProc=WndProc;
	WndClass.hInstance=Instance;
	WndClass.hIcon=LoadIcon(Instance,MAKEINTRESOURCE(IDI_81GYAZOWIN));
	WndClass.hIconSm=(HICON)LoadImage(Instance,MAKEINTRESOURCE(IDI_81GYAZOWIN),IMAGE_ICON,16,16,0);
	WndClass.hCursor=LoadCursor(Instance,MAKEINTRESOURCE(IDC_CURSOR1));//十字カーソル
	WndClass.hbrBackground=0;//背景も設定しない
	WndClass.lpszClassName=WindowClass;

	RegisterClassEx(&WndClass);

	//レイヤーウィンドウ
	WndClass.style=CS_HREDRAW|CS_VREDRAW;
	WndClass.lpfnWndProc=LayerWndProc;
	WndClass.hInstance=Instance;
	WndClass.hbrBackground=(HBRUSH)Brush.get();
	WndClass.lpszClassName=WindowClassL;
	
	RegisterClassEx(&WndClass);
	return; 
}

//インスタンスの初期化（全画面をウィンドウで覆う）
bool InitInstance(HINSTANCE Instance)
{
	HWND WindowHandle;
	int X,Y,Width,Height;

	//仮想スクリーン全体をカバー
	X=GetSystemMetrics(SM_XVIRTUALSCREEN);
	Y=GetSystemMetrics(SM_YVIRTUALSCREEN);
	Width=GetSystemMetrics(SM_CXVIRTUALSCREEN);
	Height=GetSystemMetrics(SM_CYVIRTUALSCREEN);

	//x,yのオフセット値を覚えておく
	OffsetX=X;
	OffsetY=Y;

	//完全に透過したウィンドウを作る
	WindowHandle=CreateWindowEx(WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,
		WindowClass,nullptr,WS_POPUP,0,0,0,0,
		nullptr,nullptr,Instance,nullptr);

	//作れなかった...?
	if(!WindowHandle) return false;
	
	//全画面を覆う
	MoveWindow(WindowHandle,X,Y,Width,Height,false);
	
	ShowWindow(WindowHandle,SW_SHOW);
	UpdateWindow(WindowHandle);

	//ESCキー検知タイマー
	SetTimer(WindowHandle,1,100,nullptr);
	
	//レイヤーウィンドウの作成
	LayerWindowHandle=CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_NOACTIVATE,
		WindowClassL,nullptr,WS_POPUP,0,0,0,0,
		WindowHandle,nullptr,Instance,nullptr);

	//作れなかった...?
	if(!LayerWindowHandle) return false;

	SetLayeredWindowAttributes(LayerWindowHandle,RGB(255,0,0),100,LWA_COLORKEY|LWA_ALPHA);
	return true;
}

//指定されたフォーマットに対応するEncoderのCLSIDを取得する
bool GetEncoderClassID(const std::wstring& MimeType,CLSID* ClassID)
{
	typedef Gdiplus::ImageCodecInfo CodecInfo;
	unsigned EncoderCount,Size;

	Gdiplus::GetImageEncodersSize(&EncoderCount,&Size);
	if(!Size) return false;

	std::vector<unsigned char> Codecs(Size,0);

	Gdiplus::GetImageEncoders(EncoderCount,Size,(CodecInfo*)Codecs.data());

	auto Codec=std::find_if((CodecInfo*)Codecs.data(),(CodecInfo*)Codecs.data()+EncoderCount,
		[&](CodecInfo& Codec){return !MimeType.compare(Codec.MimeType);});

	std::memcpy(ClassID,&(*Codec).Clsid,sizeof(CLSID));
	return true;
}

//ヘッダを見てPNG画像かどうか(一応)チェック
bool IsPNG(const std::wstring& FileName)
{
	std::array<unsigned char,8> PNGHeader={0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A},ReadHeader={0};	
	std::ifstream File(FileName,std::ios::in|std::ios::binary);
	File.read((char*)ReadHeader.data(),ReadHeader.size());
	File.close();
	return ReadHeader==PNGHeader;
}

//PNG形式に変換
bool ConvertPNG(const std::wstring& DestFile,const std::wstring& SrcFile)
{
	CLSID ClassID;
	Gdiplus::Bitmap SrcImage(SrcFile.c_str());

	if(SrcImage.GetLastStatus()==Gdiplus::Status::Ok&&GetEncoderClassID(L"image/png",&ClassID))
		return SrcImage.Save(DestFile.c_str(),&ClassID)==Gdiplus::Status::Ok?true:false;
	else return false;
}

//PNG形式で保存
bool SavePNG(const std::wstring& FileName,HBITMAP NewBitmap)
{
	CLSID ClassID;
	//HBITMAPからBitmapを作成
	Gdiplus::Bitmap Image(NewBitmap,(HPALETTE)GetStockObject(DEFAULT_PALETTE));
	
	if(GetEncoderClassID(L"image/png",&ClassID))
		return Image.Save(FileName.c_str(),&ClassID)==Gdiplus::Status::Ok?true:false;
	else return false;
}

//ラバーバンドを描画
void DrawRubberband(RECT& NewRect)
{
	MoveWindow(LayerWindowHandle,
		NewRect.left<NewRect.right?NewRect.left:NewRect.right,
		NewRect.top<NewRect.bottom?NewRect.top:NewRect.bottom,
		std::abs(NewRect.right-NewRect.left)+1,
		std::abs(NewRect.bottom-NewRect.top)+1,true);
	return;
}


//レイヤーウィンドウプロシージャ
LRESULT __stdcall LayerWndProc(HWND WindowHandle,UINT Message,WPARAM WParam,LPARAM LParam)
{
	switch (Message){
	case WM_PAINT:
		{
			PAINTSTRUCT PaintStruct;
			BeginPaint(WindowHandle,&PaintStruct);
			std::unique_ptr<Gdiplus::Font> DrawFont(new Gdiplus::Font(L"Tahoma",8));
			std::unique_ptr<Gdiplus::SolidBrush> WhiteBrush(new Gdiplus::SolidBrush(Gdiplus::Color::White)),BlackBrush(new Gdiplus::SolidBrush(Gdiplus::Color::Black));
			std::unique_ptr<Gdiplus::Graphics> LayerWindowGraphics(new Gdiplus::Graphics(WindowHandle));
			std::unique_ptr<Gdiplus::StringFormat> Layout(new Gdiplus::StringFormat(Gdiplus::StringFormatFlagsNoWrap));
			Layout->SetAlignment(Gdiplus::StringAlignment::StringAlignmentFar);
			Layout->SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentFar);
			Layout->SetTrimming(Gdiplus::StringTrimmingNone);
			RECT ClientRect;
			GetClientRect(WindowHandle,&ClientRect);
			Gdiplus::RectF LayoutRect(ClientRect.left,ClientRect.top,ClientRect.right-5,ClientRect.bottom-5);
			std::vector<wchar_t> WindowSize(64,0);
			_itow(ClientRect.right,WindowSize.data(),10);
			auto Null=std::find(WindowSize.begin(),WindowSize.end(),0);
			*Null++=L'\n';
			_itow(ClientRect.bottom,Null._Ptr,10);
			int Count=std::wcslen(WindowSize.data());
			LayerWindowGraphics->DrawString(WindowSize.data(),Count,&*DrawFont,LayoutRect,&*Layout,&*BlackBrush);
			LayoutRect.Inflate(-1,-1);
			LayerWindowGraphics->DrawString(WindowSize.data(),Count,&*DrawFont,LayoutRect,&*Layout,&*WhiteBrush);
			EndPaint(WindowHandle,&PaintStruct);
			break;
		}
	default:
		return DefWindowProc(WindowHandle,Message,WParam,LParam);
	}
	return 0;

}

//ウィンドウプロシージャ
LRESULT __stdcall WndProc(HWND WindowHandle,UINT Message,WPARAM WParam,LPARAM LParam)
{
	static bool CaptureStarted=false;
	static RECT CaptureRect={0};
	
	switch(Message){
	case WM_RBUTTONDOWN:
		//キャンセル
		DestroyWindow(WindowHandle);
		return DefWindowProc(WindowHandle,Message,WParam,LParam);
		break;
	case WM_TIMER:
		//ESCキー押下の検知
		if(GetKeyState(VK_ESCAPE)&0x8000){
			DestroyWindow(WindowHandle);
			return DefWindowProc(WindowHandle,Message,WParam,LParam);
		}
		break;
	case WM_MOUSEMOVE:
		if(CaptureStarted){
			//新しい座標をセット
			CaptureRect.right=LOWORD(LParam)+OffsetX;
			CaptureRect.bottom=HIWORD(LParam)+OffsetY;
			DrawRubberband(CaptureRect);
		}
		break;
	case WM_LBUTTONDOWN:
		//クリップ開始
		CaptureStarted=true;
		ShowWindow(LayerWindowHandle,SW_SHOW);
		
		//初期位置をセット
		CaptureRect.left=LOWORD(LParam)+OffsetX;
		CaptureRect.top=HIWORD(LParam)+OffsetY;

		//マウスをキャプチャ
		SetCapture(WindowHandle);
		break;
	case WM_LBUTTONUP:
		{
			//クリップ終了
			CaptureStarted=false;
			ShowWindow(LayerWindowHandle,SW_HIDE);
			
			//マウスのキャプチャを解除
			ReleaseCapture();
		
			//新しい座標をセット
			CaptureRect.right=LOWORD(LParam)+OffsetX;
			CaptureRect.bottom=HIWORD(LParam)+OffsetY;

			HDC Hdc,NewHdc;
			HBITMAP NewBitmap;
			Hdc=GetDC(nullptr);

			//画像のキャプチャ
			int Width,Height;
			Width=std::abs(CaptureRect.right-CaptureRect.left+1);
			Height=std::abs(CaptureRect.bottom-CaptureRect.top+1);

			if(!Width||!Height) {
				//画像になってないので何もしない
				ReleaseDC(nullptr,Hdc);
				DestroyWindow(WindowHandle);
				break;
			}

			// ビットマップバッファを作成
			NewBitmap=CreateCompatibleBitmap(Hdc,Width,Height);
			NewHdc=CreateCompatibleDC(Hdc);
			
			//関連づけ
			SelectObject(NewHdc,NewBitmap);

			//画像を取得
			BitBlt(NewHdc,0,0,Width,Height,Hdc,
				CaptureRect.left<CaptureRect.right?CaptureRect.left:CaptureRect.right,
				CaptureRect.top<CaptureRect.bottom?CaptureRect.top:CaptureRect.bottom,SRCCOPY);
			
			//ウィンドウを隠す
			ShowWindow(WindowHandle,SW_HIDE);
			
			//テンポラリファイル名を決定
			std::vector<wchar_t> TempDirectory(MAX_PATH,0),TempFile(MAX_PATH,0);
			GetTempPath(MAX_PATH,TempDirectory.data());
			GetTempFileName(TempDirectory.data(),TempPrefix,0,TempFile.data());
			
			if(SavePNG(TempFile.data(),NewBitmap)) UploadFile(TempFile.data());
			else MessageBox(WindowHandle,L"一時ファイルの保存に失敗しました。",Title,MB_OK|MB_ICONERROR);

			//後始末
			DeleteFile(TempFile.data());
			DeleteDC(NewHdc);
			DeleteObject(NewBitmap);
			ReleaseDC(nullptr,Hdc);
			DestroyWindow(WindowHandle);
		}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(WindowHandle,Message,WParam,LParam);
	}
	return 0;
}

// クリップボードに文字列をコピー
void SetClipBoardText(const std::wstring& String)
{

	HGLOBAL TextHandle;
	wchar_t* Text;

	TextHandle=GlobalAlloc(GMEM_DDESHARE|GMEM_MOVEABLE,(String.length()+1)*sizeof(wchar_t));

	Text=(wchar_t*)GlobalLock(TextHandle);
	std::wcscpy(Text,String.c_str());
	GlobalUnlock(TextHandle);
	
	//クリップボードを開く
	OpenClipboard(nullptr);
	EmptyClipboard();
	SetClipboardData(CF_TEXT,TextHandle);
	CloseClipboard();

	//解放
	GlobalFree(TextHandle);
	return;
}

//指定されたURLをブラウザで開く
void OpenUrl(const std::wstring& Url)
{
	//コマンドを実行
	SHELLEXECUTEINFO ExecuteInfo={0};
	ExecuteInfo.cbSize=sizeof(SHELLEXECUTEINFO);
	ExecuteInfo.lpVerb=L"open";
	ExecuteInfo.lpFile=Url.c_str();
	ShellExecuteEx(&ExecuteInfo);
	return;
}

//IDを生成・ロードする
std::string GetID()
{
	std::vector<wchar_t> IDFile(MAX_PATH,0),IDDirectory(MAX_PATH,0);
	std::string IDString;
	bool OldIDFileExist=false;

	SHGetSpecialFolderPath(nullptr,IDFile.data(),CSIDL_APPDATA,false);
	std::wcscat(IDFile.data(),L"\\81Gyazo");
	std::wcscpy(IDDirectory.data(),IDFile.data());
	std::wcscat(IDFile.data(),L"\\id.txt");

	//まずはファイルから ID をロード
	std::ifstream IDFileStream;

	IDFileStream.open(IDFile.data());
	if(!!IDFileStream){
		//IDを読み込む
		IDFileStream>>IDString;
		IDFileStream.close();
	}
	return IDString;
}

//IDを保存する
bool SaveID(const std::wstring& IDString)
{
	std::vector<wchar_t> IDFile(MAX_PATH,0),IDDirectory(MAX_PATH,0);

	SHGetSpecialFolderPath(nullptr,IDFile.data(),CSIDL_APPDATA,false);
	std::wcscat(IDFile.data(),L"\\81Gyazo");
	std::wcscpy(IDDirectory.data(),IDFile.data());
	std::wcscat(IDFile.data(),L"\\id.txt");

	//IDを保存する
	CreateDirectory(IDDirectory.data(),nullptr);
	std::ofstream IDFileStream;
	IDFileStream.open(IDFile.data());
	if(!!IDFileStream){
		IDFileStream<<ToMultiByte(IDString);
		IDFileStream.close();
	}else return false;
	return true;
}

// PNG ファイルをアップロードする.
bool UploadFile(const std::wstring& FileName)
{
	const bool ShowBrowser=!(GetKeyState(VK_SHIFT)&0x8000);
	const std::string Boundary="----BOUNDARYBOUNDARY----",NewLine="\r\n";
	const std::wstring Header=L"Content-Type: multipart/form-data; boundary="+ToUnicode(Boundary);
	std::ostringstream MessageStream;//送信メッセージ
	std::string IDString=GetID();//ID
	std::string Template="--"+Boundary+NewLine+"Content-Disposition: form-data; name=";

	//メッセージの構成
	//id part
	MessageStream<<Template<<"\"id\""<<NewLine<<NewLine<<IDString<<NewLine;
	//imagedata part
	MessageStream<<Template<<"\"imagedata\"; filename=\""+ToMultiByte(UploadServer)+"\""<<NewLine<<"Content-Type: image/png"<<NewLine<<NewLine;
	//PNGファイルを読み込む
	std::ifstream PNGFile;
	PNGFile.open(FileName,std::ios::in|std::ios::binary);
	if(!PNGFile){
		MessageBox(nullptr,L"画像ファイルを開く事が出来ませんでした。",Title,MB_ICONERROR|MB_OK);
		PNGFile.close();
		return false;
	}
	MessageStream<<PNGFile.rdbuf();//全部読み込んでバッファに追加する
	PNGFile.close();

	//最後
	MessageStream<<NewLine<<"--"<<Boundary<<"--"<<NewLine;

	//メッセージ完成
	std::string OutputMessage(MessageStream.str());

	auto DestroyFunction=[](void* Obj){if(Obj) InternetCloseHandle(Obj);};
	typedef std::shared_ptr<void> Handle;

	//WinInet を準備（proxyは既定の設定を利用）
	Handle SessionHandle(InternetOpen(Title,INTERNET_OPEN_TYPE_PRECONFIG,nullptr,nullptr,0),DestroyFunction);
	if(!SessionHandle){
		MessageBox(nullptr,L"接続の初期化に失敗しました。",Title,MB_ICONERROR|MB_OK);
		return false;
	}
	
	// 接続先
	Handle ConnectionHandle(InternetConnect(SessionHandle.get(),UploadServer,80,nullptr,nullptr,INTERNET_SERVICE_HTTP,0,0),DestroyFunction);
	if(!ConnectionHandle){
		MessageBox(nullptr,L"接続を開始する事が出来ませんでした。",Title,MB_ICONERROR|MB_OK);
		return false;
	}

	//要求先の設定
	Handle RequestHandle(HttpOpenRequest(ConnectionHandle.get(),L"POST",ScriptName,nullptr,nullptr,nullptr,INTERNET_FLAG_NO_CACHE_WRITE,0),DestroyFunction);
	if(!RequestHandle){
		MessageBox(nullptr,L"リクエストの構成に失敗しました。",Title,MB_ICONERROR|MB_OK);
		return false;
	}

	//User-Agentを指定
	const std::wstring UserAgent=L"User-Agent: 81Gyazo_win\r\n";
	bool Result=HttpAddRequestHeaders(RequestHandle.get(),UserAgent.c_str(),UserAgent.length(),HTTP_ADDREQ_FLAG_ADD|HTTP_ADDREQ_FLAG_REPLACE);
	if(!Result){
		MessageBox(nullptr,L"ユーザーエージェントを設定出来ませんでした。",Title,MB_ICONERROR|MB_OK);
		return false;
	}

	//要求を送信
	if(HttpSendRequest(RequestHandle.get(),Header.c_str(),Header.length(),(void*)OutputMessage.c_str(),OutputMessage.length())){
		//要求は成功
		std::vector<wchar_t> StatusCode(8,0);
		DWORD Length=StatusCode.size();

		//status codeを取得
		HttpQueryInfo(RequestHandle.get(),HTTP_QUERY_STATUS_CODE,StatusCode.data(),&Length,nullptr);
		if(_wtoi(StatusCode.data())!=200){
			//アップロード失敗(status error)
			std::wstring ErrorStr=L"アップロードに失敗しました。\nサーバーがメンテナンス中の可能性があります。\n(ステータスコード:";
			ErrorStr+=StatusCode.data()+(std::wstring)L")";
			MessageBox(nullptr,ErrorStr.c_str(),Title,MB_ICONERROR|MB_OK);
			return false;
		}else{
			//アップロード成功

			//ID取得
			std::vector<wchar_t> NewID(100,0);
			DWORD Length=NewID.size();
			std::wcscpy(NewID.data(),L"X-Gyazo-Id");

			HttpQueryInfo(RequestHandle.get(),HTTP_QUERY_CUSTOM,NewID.data(),&Length,0);
			if(GetLastError()!=ERROR_HTTP_HEADER_NOT_FOUND&&Length)	SaveID(NewID.data());

			//結果を読取る
			std::vector<char> ResultBuffer(1024,0);
			std::string ResultString;
			
			//そんなに長いことはないけどまあ一応
			while(InternetReadFile(RequestHandle.get(),(void*)ResultBuffer.data(),ResultBuffer.size(),&Length)&&Length)	ResultString.append(ResultBuffer.data(),Length);
			
			//取得結果はNULL文字で終わってないので
			ResultString+='\0';

			//クリップボードにURLをコピー
			auto Url=ToUnicode(ResultString);
			SetClipBoardText(Url);
			
			//URLを起動
			if(ShowBrowser) OpenUrl(Url);
			else MessageBox(nullptr,L"アップロードが完了しました。",Title,MB_ICONINFORMATION|MB_OK);
			return true;
		}
	}else{
		//アップロード失敗...
		MessageBox(nullptr,L"アップロードに失敗しました。",Title,MB_ICONERROR|MB_OK);
		return false;
	}

}

std::wstring ToUnicode(const std::string& MultiByteString)
{
	std::vector<wchar_t> TempBuffer(MultiByteString.length()+1,0);
	std::mbstowcs(TempBuffer.data(),MultiByteString.c_str(),MultiByteString.length());
	return std::wstring(TempBuffer.data());
}

std::wstring ToUnicode(const std::vector<char>& MultiByteBuffer)
{
	std::vector<wchar_t> TempBuffer(MultiByteBuffer.size(),0);
	std::mbstowcs(TempBuffer.data(),MultiByteBuffer.data(),MultiByteBuffer.size());
	return std::wstring(TempBuffer.data());
}

std::string ToMultiByte(const std::wstring& UnicodeString)
{
	std::vector<char> TempBuffer(UnicodeString.length()+1,0);
	std::wcstombs(TempBuffer.data(),UnicodeString.c_str(),UnicodeString.length());
	return std::string(TempBuffer.data());
}

std::string ToMultiByte(const std::vector<wchar_t>& UnicodeBuffer)
{
	std::vector<char> TempBuffer(UnicodeBuffer.size(),0);
	std::wcstombs(TempBuffer.data(),UnicodeBuffer.data(),UnicodeBuffer.size());
	return std::string(TempBuffer.data());
}