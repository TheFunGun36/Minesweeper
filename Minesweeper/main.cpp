#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <ctime>
#include <cmath>
#include <vector>
#include <sstream>
#include <queue>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#define TIMER_ID 321
#define SafeRelease(p) if (p != NULL) p->Release(); p = NULL;
#define MY_WINDOW (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU)
constexpr float g_cellSize = 50.0f;
constexpr float g_marginInBetween = 2.0f;
constexpr float g_marginLeft = 25.0f;
constexpr float g_marginRight = 25.0f;
constexpr float g_marginTop = 25.0f;
constexpr float g_marginBottom = 25.0f;
constexpr float g_panelSize = 100.0f;


namespace d2d {
    int wndSizeX, wndSizeY;

    PAINTSTRUCT ps;
    ID2D1HwndRenderTarget* pRenderTarget = NULL;
    ID2D1SolidColorBrush* pCellIdleBrush = NULL;
    ID2D1SolidColorBrush* pCellHoveredBrush = NULL;
    ID2D1SolidColorBrush* pCellPressedBrush = NULL;
    ID2D1SolidColorBrush* pCellEmptyBrush = NULL;
    ID2D1SolidColorBrush* pCellMarkedBrush = NULL;

    ID2D1SolidColorBrush* pNumberBrush = NULL;
    ID2D1Factory* pFactory = NULL;

    IDWriteTextFormat* pTextFormat = NULL;
    IDWriteFactory* pWriteFactory = NULL;

    bool CreateDeviceIndependent(HWND hWnd) {

        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory))) {
            DestroyWindow(hWnd);
            return true;
        }

        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pWriteFactory)))) {
            DestroyWindow(hWnd);
            return true;
        }

        if (FAILED(pWriteFactory->CreateTextFormat(L"Lucidia Console", NULL,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 36.0f, L"Russia", &pTextFormat))) {
            DestroyWindow(hWnd);
            return true;
        }
    }

    void CreateDeviceDependent(HWND hWnd) {
        RECT rc;
        GetClientRect(hWnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right-rc.left, rc.bottom-rc.top);
        wndSizeX = rc.right-rc.left;
        wndSizeY = rc.bottom-rc.top;

        if (FAILED(pFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hWnd, size),
            &pRenderTarget))) {
            DestroyWindow(hWnd);
            return;
        }

        if (FAILED(pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.5f), &pCellIdleBrush))) {
            DestroyWindow(hWnd);
            return;
        }
        if (FAILED(pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.65f, 0.65f, 0.65f), &pCellHoveredBrush))) {
            DestroyWindow(hWnd);
            return;
        }
        if (FAILED(pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.8f, 0.8f), &pCellPressedBrush))) {
            DestroyWindow(hWnd);
            return;
        }
        if (FAILED(pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f), &pCellEmptyBrush))) {
            DestroyWindow(hWnd);
            return;
        }
        if (FAILED(pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.9f, 0.6f), &pCellMarkedBrush))) {
            DestroyWindow(hWnd);
            return;
        }
        if (FAILED(pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f), &pNumberBrush))) {
            DestroyWindow(hWnd);
            return;
        }
    }

    void DestroyResources() {
        SafeRelease(pCellIdleBrush);
        SafeRelease(pCellHoveredBrush);
        SafeRelease(pCellPressedBrush);
        SafeRelease(pCellEmptyBrush);
        SafeRelease(pCellMarkedBrush);
        SafeRelease(pNumberBrush);
        SafeRelease(pRenderTarget);
    }

    void BeginDraw(HWND hWnd) {
        BeginPaint(hWnd, &ps);
        if (pRenderTarget==NULL) {
            CreateDeviceDependent(hWnd);
        }

        pRenderTarget->BeginDraw();
    }
    void EndDraw(HWND hWnd) {
        HRESULT hr = pRenderTarget->EndDraw();
        if (FAILED(hr)||hr==D2DERR_RECREATE_TARGET) {
            DestroyWindow(hWnd);
        }
        EndPaint(hWnd, &ps);
    }
}

namespace l {
#define S_IDLE 0
#define S_HOVERED 1
#define S_PRESSED 2
#define S_OPENED 3
#define S_MARKED 4

#define N_BOMB 15

    POINT mousePos;

    int gridSizeX, gridSizeY;
    int bombNumber;
    int gridSize;
    bool isLost = false;
    bool isWon = false;

    HWND hWnd;

    int closedCellsCounter = 0;

    namespace restartButton {
        namespace {
            float posX = gridSizeX*(g_cellSize+g_marginInBetween)+g_marginLeft+g_marginRight-175.0f,
                posY = g_marginTop+g_marginBottom+l::gridSizeY*(g_cellSize+g_marginInBetween)+25.0f;
            byte state;
            std::wstring text;

            bool isMouseIn(RECT r) {
                return (mousePos.x>=r.left)&&(mousePos.x<=r.right)&&(mousePos.y>=r.top)&&(mousePos.y<=r.bottom);
            }
        }

        void setPosition(float x, float y) {
            posX = x;
            posY = y;
        }

        void render() {
            ID2D1SolidColorBrush** brush;

            switch (state) {
            case S_HOVERED:
                brush = &d2d::pCellHoveredBrush;
                break;
            case S_PRESSED:
                brush = &d2d::pCellPressedBrush;
                break;
            default:
                brush = &d2d::pCellIdleBrush;
                break;
            }

            d2d::pRenderTarget->FillRectangle(D2D1::RectF(posX, posY, posX+150.0f, posY+50.0f), *brush);

            d2d::pRenderTarget->DrawTextW(L"Рестарт", 7, d2d::pTextFormat, D2D1::RectF(posX+10.0f, posY-3.0f, posX+150.0f, posY+50.0f), d2d::pNumberBrush);
        }

        void callMouseDown() {
            if (state==S_HOVERED) {
                state = S_PRESSED;
            }
        }

        bool callMouseUp() {
            bool t = false;
            if (state==S_PRESSED) {
                t = true;
                state = S_HOVERED;
            }
            return t;
        }

        void callMouseMove() {
            if (isMouseIn({ (long)posX, (long)posY, (long)posX+150, (long)posY+50 })) {
                state = S_HOVERED;
            }
            else state = S_IDLE;
        }
    };

    class Parameter {
    private:
        float posX, posY;
        byte state;
        short value;
        short minVal, maxVal;
        float textOffset;
        std::wstring text;

        bool isMouseIn(RECT r) {
            return (mousePos.x>=r.left)&&(mousePos.x<=r.right)&&(mousePos.y>=r.top)&&(mousePos.y<=r.bottom);
        }

#define PS_IDLE '\x00'
#define PS_P_HOVERED '\x01'
#define PS_M_HOVERED '\x02'
#define PS_P_PRESSED '\x03'
#define PS_M_PRESSED '\x04'

    public:
        void render() {

            ID2D1SolidColorBrush** pBrush;
            if (state==PS_M_HOVERED) pBrush = &d2d::pCellHoveredBrush;
            else if (state==PS_M_PRESSED) pBrush = &d2d::pCellPressedBrush;
            else pBrush = &d2d::pCellIdleBrush;

            d2d::pRenderTarget->FillRectangle(D2D1::RectF(posX, posY, posX+25.0f, posY+25.0f), *pBrush);
            d2d::pRenderTarget->DrawTextW(L"-", 1, d2d::pTextFormat, D2D1::RectF(posX+5.5f, posY-16.0f, posX+5.5f, posY-16.0f), d2d::pNumberBrush);

            pBrush = &d2d::pCellEmptyBrush;
            d2d::pRenderTarget->FillRectangle(D2D1::RectF(posX+30.0f, posY-5.0f, posX+80.0f, posY+30.0f), *pBrush);
            std::wstringstream sstr;
            if (value<10) sstr<<'0';
            sstr<<value;
            sstr<<'\0';
            d2d::pRenderTarget->DrawTextW(sstr.str().c_str(), 2, d2d::pTextFormat, D2D1::RectF(posX+36.0f, posY-13.0f, posX+80.0f, posY+30.0f), d2d::pNumberBrush);


            if (state==PS_P_HOVERED) pBrush = &d2d::pCellHoveredBrush;
            else if (state==PS_P_PRESSED) pBrush = &d2d::pCellPressedBrush;
            else pBrush = &d2d::pCellIdleBrush;

            d2d::pRenderTarget->FillRectangle(D2D1::RectF(posX+85.0f, posY, posX+110.0f, posY+25.0f), *pBrush);
            d2d::pRenderTarget->DrawTextW(L"+", 1, d2d::pTextFormat, D2D1::RectF(posX+85.5f, posY-16.0f, posX+90.5f, posY-16.0f), d2d::pNumberBrush);

            d2d::pRenderTarget->DrawTextW(text.c_str(), text.size(), d2d::pTextFormat, D2D1::RectF(posX+textOffset, posY-50.0f, posX+200.0f+textOffset, posY-16.0f), d2d::pCellPressedBrush);
        }

        void callMouseDown() {
            if (state==PS_M_HOVERED) {
                state = PS_M_PRESSED;
            }
            else if (state==PS_P_HOVERED) {
                state = PS_P_PRESSED;
            }
            else state = PS_IDLE;
        }

        void callMouseUp() {
            if (state==PS_M_PRESSED) {
                if (value>minVal) {
                    value--;
                }
                state = PS_M_HOVERED;
            }
            else if (state==PS_P_PRESSED) {
                if (value<maxVal&&value<99) {
                    value++;
                }
                state = PS_P_HOVERED;
            }
        }

        void callMouseMove() {
            if (isMouseIn({ (long)posX, (long)posY, (long)posX+25, (long)posY+25 })) {
                state = PS_M_HOVERED;
            }
            else if (isMouseIn({ (long)posX+85, (long)posY, (long)posX+110, (long)posY+25 })) {
                state = PS_P_HOVERED;
            }
            else state = PS_IDLE;
        }

        short getValue() {
            return value;
        }

        void setRange(short minVal, short maxVal) {
            this->minVal = minVal;
            this->maxVal = maxVal;
            if (value<minVal)
                value = minVal;
            if (value>maxVal) {
                value = maxVal;
            }
        }

        void setPosition(float x, float y) {
            posX = x;
            posY = y;
        }

        Parameter(float x, float y, short value = 10, short minVal = 5, short maxVal = 15, float textOffset = 5.0f, std::wstring text = L"Мины") {
            posX = x;
            posY = y;
            state = PS_IDLE;
            this->value = value;
            this->minVal = minVal;
            this->maxVal = maxVal;
            this->textOffset = textOffset;
            this->text = text;
        }

#undef PS_IDLE 
#undef PS_P_HOVERE
#undef PS_M_HOVERED
#undef PS_P_PRESSED
#undef PS_M_PRESSED
    };

    byte* cell;
    int chosenCell;
    int openedCells;

    Parameter bombNumberParam(0.0f, 0.0f, 10, 5, 20);
    Parameter gridSizeXParam(0.0f, 0.0f, 12, 12, 35, -14.0f, L"Ширина");
    Parameter gridSizeYParam(0.0f, 0.0f, 10, 5, 15, -3.5f, L"Высота");

    short getCellNumber(int id) {
        return cell[id]&0x0f;
    }

    void setCellNumber(int id, short number) {
        cell[id] &= 0xf0;
        cell[id] |= number;
    }

    short getCellState(int id) {
        return (cell[id]&0x70)>>4;
    }

    void setCellState(int id, short state) {
        cell[id] &= 0x8f;
        cell[id] |= state<<4;
    }

    inline int getCellId(short x, short y) {
        if ((x>=0)&&(x<gridSizeX)&&(y>=0)&&(y<gridSizeY)) {
            return y*gridSizeX+x;
        }
        else return -1;
    }

    inline void getCellPos(int id, short* x, short* y) {
        *x = id%gridSizeX;
        *y = id/gridSizeX;
    }

    ID2D1SolidColorBrush* getCellColor(int id) {
        short t = getCellState(id);
        switch (t) {
        case S_IDLE:
            return d2d::pCellIdleBrush;
            break;
        case S_HOVERED:
            return d2d::pCellHoveredBrush;
            break;
        case S_PRESSED:
            return d2d::pCellPressedBrush;
            break;
        case S_OPENED:
            return d2d::pCellEmptyBrush;
            break;
        case S_MARKED:
            return d2d::pCellMarkedBrush;
            break;
        default:
            return d2d::pCellIdleBrush;
        }
    }

    void setHwnd(HWND hwnd) {
        hWnd = hwnd;
    }

    void initialize() {
        isWon = false;
        isLost = false;
        gridSizeX = gridSizeXParam.getValue();
        gridSizeY = gridSizeYParam.getValue();
        gridSize = gridSizeX*gridSizeY;
        bombNumber = bombNumberParam.getValue();

        closedCellsCounter = gridSize-bombNumber;

        srand(clock());
        cell = new byte[gridSize];
        memset(cell, (byte)(0), gridSize*sizeof(byte));
        chosenCell = -1;

        for (int i = 0; i<bombNumber; i++) {
            short randomId = rand()%gridSize;

            while (getCellNumber(randomId)==0x0f) {
                randomId = rand()%gridSize;
            }

            setCellNumber(randomId, N_BOMB);

            short x, y;
            getCellPos(randomId, &x, &y);

            {
                int id;
                if (y>0) {
                    id = getCellId(x, y-1);
                    if (getCellNumber(id)!=0x0f) cell[id]++;
                }
                if (y<gridSizeY-1) {
                    id = getCellId(x, y+1);
                    if (getCellNumber(id)!=0x0f) cell[id]++;
                }
                if (x>0) {
                    id = getCellId(x-1, y);
                    if (getCellNumber(id)!=0x0f) cell[id]++;
                    if (y>0) {
                        id = getCellId(x-1, y-1);
                        if (getCellNumber(id)!=0x0f) cell[id]++;
                    }
                    if (y<gridSizeY-1) {
                        id = getCellId(x-1, y+1);
                        if (getCellNumber(id)!=0x0f) cell[id]++;
                    }
                }
                if (x<gridSizeX-1) {
                    id = getCellId(x+1, y);
                    if (getCellNumber(id)!=0x0f) cell[id]++;
                    if (y>0) {
                        id = getCellId(x+1, y-1);
                        if (getCellNumber(id)!=0x0f) cell[id]++;
                    }
                    if (y<gridSizeY-1) {
                        id = getCellId(x+1, y+1);
                        if (getCellNumber(id)!=0x0f) cell[id]++;
                    }
                }
            }
        }

        RECT r;

        r.left = 0;
        r.top = 0;
        r.right = g_marginLeft+g_marginRight+(g_cellSize+g_marginInBetween)*gridSizeX;
        r.bottom = g_marginTop+g_marginBottom+(g_cellSize+g_marginInBetween)*gridSizeY+g_panelSize;

        {
            RECT rr;
            GetWindowRect(hWnd, &rr);
            r.left += rr.left+8;
            r.right += rr.left+8;
            r.top += rr.top+31;
            r.bottom += rr.top+31;
        }

        AdjustWindowRect(&r, MY_WINDOW, FALSE);

        SetWindowPos(hWnd, HWND_TOP, r.left, r.top, r.right-r.left, r.bottom-r.top, NULL);

        bombNumberParam.setPosition(25.0f, r.bottom-r.top-85.0f);
        gridSizeXParam.setPosition(190.0f, r.bottom-r.top-85.0f);
        gridSizeYParam.setPosition(355.0f, r.bottom-r.top-85.0f);
        restartButton::setPosition(r.right-r.left-185.0f, r.bottom-r.top-112.5f);

        d2d::DestroyResources();
        d2d::CreateDeviceDependent(hWnd);
    }

    void uninitialize() {
        delete[]cell;
    }

    void updateMousePosition(LPARAM lParam) {
        mousePos.x = LOWORD(lParam);
        mousePos.y = HIWORD(lParam);

        if (!isLost&&!isWon) {
            int newCell;
            {
                short x = (mousePos.x-g_marginLeft)/(g_cellSize+g_marginInBetween);
                short y = (mousePos.y-g_marginTop)/(g_cellSize+g_marginInBetween);

                if (((mousePos.x-g_marginLeft)<0.0f)||
                    ((mousePos.y-g_marginTop)<0.0f)) {
                    newCell = -1;
                }
                else {
                    newCell = getCellId(x, y);
                }
            }

            if (newCell!=chosenCell) {

                if (chosenCell!=-1) {
                    if (getCellState(chosenCell)!=S_OPENED&&getCellState(chosenCell)!=S_MARKED) {
                        setCellState(chosenCell, S_IDLE);
                    }
                }

                chosenCell = newCell;

                if (chosenCell!=-1) {
                    if (getCellState(chosenCell)!=S_OPENED&&getCellState(chosenCell)!=S_MARKED) {
                        setCellState(chosenCell, S_HOVERED);
                    }
                }
            }
        }

        bombNumberParam.callMouseMove();
        gridSizeXParam.callMouseMove();
        gridSizeYParam.callMouseMove();
        restartButton::callMouseMove();
    }

    void mouseDown() {
        if (!isLost&&!isWon) {
            if (chosenCell!=-1) {
                if (getCellState(chosenCell)!=S_OPENED&&getCellState(chosenCell)!=S_MARKED) {
                    setCellState(chosenCell, S_PRESSED);
                }
            }
        }
        bombNumberParam.callMouseDown();
        gridSizeXParam.callMouseDown();
        gridSizeYParam.callMouseDown();
        restartButton::callMouseDown();
    }

    void mouseUp() {
        if (!isLost&&!isWon) {
            if (chosenCell!=-1&&getCellState(chosenCell)==S_PRESSED) {
                setCellState(chosenCell, S_OPENED);

                if (getCellNumber(chosenCell)==N_BOMB) {
                    for (int i = 0; i<gridSize; i++) {
                        if (getCellNumber(i)==N_BOMB) {
                            setCellState(i, S_OPENED);
                        }
                    }
                    isLost = true;
                }
                else {
                    closedCellsCounter--;

                    if (getCellNumber(chosenCell)==0) {

                        std::queue<int> toUpdate;

                        toUpdate.push(chosenCell);

                        while (!toUpdate.empty()) {
                            int current = toUpdate.front();
                            toUpdate.pop();

                            short x, y;
                            getCellPos(current, &x, &y);

                            for (int iy = y-1; iy<=y+1; iy++) {
                                for (int ix = x-1; ix<=x+1; ix++) {
                                    int id = getCellId(ix, iy);
                                    if (id!=-1&&getCellState(id)!=S_OPENED) {
                                        setCellState(id, S_OPENED);
                                        closedCellsCounter--;

                                        if (getCellNumber(id)==0) {
                                            toUpdate.push(id);
                                        }
                                    }
                                }
                            }

                        }
                    }
                    
                    if (closedCellsCounter==0) {
                        for (int i = 0; i<gridSize; i++) {
                            if (getCellState(i)!=S_OPENED) {
                                setCellState(i, S_MARKED);
                            }
                        }
                        isWon = true;
                    }
                }
            }
        }
        bombNumberParam.callMouseUp();
        gridSizeXParam.callMouseUp();
        gridSizeYParam.callMouseUp();

        bombNumberParam.setRange(5, gridSizeXParam.getValue()*gridSizeYParam.getValue()/5);

        if (restartButton::callMouseUp()) {
            uninitialize();
            initialize();
        }
    }

    void rMouseDown() {
        if (!isLost&&!isWon&&chosenCell!=-1) {
            if (getCellState(chosenCell)!=S_OPENED) {
                if (getCellState(chosenCell)==S_MARKED) {
                    setCellState(chosenCell, S_HOVERED);
                }
                else setCellState(chosenCell, S_MARKED);
            }
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {
    srand(clock());

    WNDCLASSEX wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = HS_VERTICAL|HS_HORIZONTAL;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(hInst, IDC_ARROW);
    wcex.lpszClassName = L"CLA";
    wcex.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wcex.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
    wcex.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);

    RegisterClassEx(&wcex);

    RECT r;
    r.left = 50;
    r.top = 50;
    r.right = 50+g_marginLeft+g_marginRight+(g_cellSize+g_marginInBetween)*12;
    r.bottom = 50+g_marginTop+g_marginBottom+(g_cellSize+g_marginInBetween)*10+g_panelSize;

    AdjustWindowRect(&r, MY_WINDOW, FALSE);

    HWND hWnd = CreateWindow(L"CLA", L"Minesweeper", MY_WINDOW, r.left, r.top, r.right-r.left, r.bottom-r.top, NULL, NULL, hInst, NULL);

    l::setHwnd(hWnd);

    l::initialize();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;

    SetTimer(hWnd, TIMER_ID, 17, NULL);

    BOOL ret = 1;
    while (ret>0) {
        ret = GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KillTimer(hWnd, TIMER_ID);

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        if (d2d::CreateDeviceIndependent(hWnd))
            return 0;
        break;

    case WM_MOUSEMOVE:
        l::updateMousePosition(lParam);
        InvalidateRect(hWnd, NULL, false);
        break;

    case WM_RBUTTONDOWN:
        l::rMouseDown();
        InvalidateRect(hWnd, NULL, false);
        break;

    case WM_LBUTTONDOWN:
        l::mouseDown();
        InvalidateRect(hWnd, NULL, false);
        break;

    case WM_LBUTTONUP:
        l::mouseUp();
        InvalidateRect(hWnd, NULL, false);
        break;

    case WM_SIZE:
        d2d::DestroyResources();
        d2d::CreateDeviceDependent(hWnd);
        //l::resize();
        break;

    case WM_TIMER:
        break;

    case WM_PAINT:
    {
        d2d::BeginDraw(hWnd);

        d2d::pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f));

        if (!l::isWon) {
            for (int id = 0; id<l::gridSize; id++) {
                short x, y;
                l::getCellPos(id, &x, &y);

                float posX = x*(g_cellSize+g_marginInBetween)+g_marginLeft;
                float posY = y*(g_cellSize+g_marginInBetween)+g_marginTop;

                d2d::pRenderTarget->FillRectangle(D2D1::RectF(posX, posY, posX+g_cellSize, posY+g_cellSize),
                    l::getCellColor(id));

                if (l::getCellState(id)==S_OPENED) {
                    WCHAR c[1]; c[0] = l::getCellNumber(id)+'0';

                    if (c[0]>'9')
                        d2d::pRenderTarget->DrawTextW(L"X", 1, d2d::pTextFormat, D2D1::RectF(15.0f+posX, posY, 15.0f+posX, posY), d2d::pNumberBrush);
                    else if (c[0]!='0')
                        d2d::pRenderTarget->DrawTextW(c, 1, d2d::pTextFormat, D2D1::RectF(15.0f+posX, posY, 15.0f+posX, posY), d2d::pNumberBrush);
                }
            }

            short panelAlignment = g_marginTop+g_marginBottom+l::gridSizeY*(g_cellSize+g_marginInBetween);

            d2d::pRenderTarget->DrawLine(D2D1::Point2F(0.0f, panelAlignment),
                D2D1::Point2F(g_marginLeft+g_marginRight+l::gridSizeX*(g_cellSize+g_marginInBetween), panelAlignment), d2d::pCellPressedBrush);

            l::bombNumberParam.render();
            l::gridSizeXParam.render();
            l::gridSizeYParam.render();
            l::restartButton::render();
        }
        else {

        }

        d2d::EndDraw(hWnd);
    }
    break;

    case WM_DESTROY:
        d2d::DestroyResources();
        SafeRelease(d2d::pTextFormat);
        SafeRelease(d2d::pWriteFactory);
        SafeRelease(d2d::pFactory);
        l::uninitialize();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
