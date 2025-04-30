#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <d3d9.h>			// Direct3D9 (DirectX9 Graphics)に必要
#include <d3dx9core.h>		// DirectX スプライトに必要
#include <tchar.h>
#include <time.h>			// srandの種に現在時刻を使うため.
#include <assert.h>

using namespace std;

// ウィンドウの幅と高さを定数で設定する
#define WIDTH 1024			//< 幅
#define HEIGHT 768			//< 高さ

// ビリヤード台の各辺の位置(壁に触れたとき、どの面に触れたかの参照に使用？)
#define BOARD_TOP -18
#define BOARD_BOTTOM 18
#define BOARD_LEFT -35
#define BOARD_RIGHT 35


// ウィンドウタイトルバーに表示されるバージョン名
static const TCHAR version[] = _TEXT(__DATE__ " " __TIME__);

// ウィンドウクラス名.
static const TCHAR wndClsName[] = _TEXT("3D Billiards");

// クラス宣言。実体の定義より先にクラス名だけを宣言しておく。
struct DataSet;
class MyXData;
class MyBall;

// 関数宣言
HRESULT initDirect3D(DataSet *data);
const TCHAR *d3dErrStr(HRESULT res);
void ReleaseDataSet(DataSet *data);

// プログラムに必要な変数を構造体として定義
struct DataSet {
	HINSTANCE hInstance;		//< インスタンスハンドル
	HWND hWnd;					//< 表示ウィンドウ
	IDirect3D9 *pD3D;			//< Direct3Dインスタンスオブジェクト
	D3DPRESENT_PARAMETERS d3dpp;//< デバイス作成時のパラメータ
	IDirect3DDevice9 *dev;		//< Direct3Dデバイスオブジェクト
	int gameTimer;				//< 全体用タイマー

	// 3D表示用のデータ
	float aspect;				//< 画面アスペクト比（縦横比）
	float cam_rotx;				//< カメラ回転量
	float cam_roty;
	float cam_transz;			//< カメラのZ位置

	// ライトのON/OFF
	bool light[4];				//< ライトのON/OFF用フラグ
	// 各種フラグ
	bool flag[10];				//< 0:SOLID/WIRE

	MyBall *ball[11];			//< ボール配列。番兵を置くので10+1個
	MyXData *raxa;				// ビリヤード台のXデータ
} mydata;

////////////////////////// class MyXData //////////////////////////
// Xデータを管理するクラス。
// メンバ関数にloadがあるが、この関数はstatic関数。つまり、クラス関数。
// インスタンスを生成するための関数となっている。
class MyXData {
private:
	// Xファイル読み込みのために必要な変数
	ID3DXMesh *pMesh_;				//< 形状データ
	DWORD numMaterials_;			//< マテリアルデータ数
	IDirect3DTexture9 **pTextures_;	//< テクスチャ情報
	D3DMATERIAL9 *pMaterials_;		//< マテリアルデータ

public:
	MyXData() :pMesh_(0), numMaterials_(0), pTextures_(0), pMaterials_(0) {}
	~MyXData();
	void draw(DataSet *data);		//< 描画

	// staticメンバ関数は、クラス関数となる。
	static MyXData *load(DataSet *data, const TCHAR *fname);
};

// デストラクタでは全データを解放する
MyXData::~MyXData() {
	if (pMesh_) pMesh_->Release();
	for(unsigned int ii = 0; ii < numMaterials_; ii++) {
		if (pTextures_[ii]) pTextures_[ii]->Release();
	}
	if (pTextures_) delete[] pTextures_;
	if (pMaterials_) delete[] pMaterials_;
}


// Xファイルを読み込む。自分自身を生成するため、staticメンバ関数となっている。
// arg data データセット
// arg fname ファイル名
MyXData *MyXData::load(DataSet *data, const TCHAR *fname) {
	MyXData *xxx = new MyXData();		// インスタンスを生成

	// 指定するXファイルを読み込んで、システムメモリに展開する。
	// ポリゴンメッシュ情報の他に、マテリアル情報も読み込む。
	HRESULT hr;
	ID3DXBuffer *pMB;		// マテリアル情報のためのデータバッファ

	// D3DXLoadMeshFromXによりXファイルからポリゴンメッシュを読み込む。
	// データ配置場所としてSYSTEMMEM(システムメモリ)を選択する。
	// これによりフルスクリーン切り替え時にもデータは破壊されない。
	hr = D3DXLoadMeshFromX(fname, D3DXMESH_SYSTEMMEM, data->dev, NULL, 
		&pMB, NULL, &xxx->numMaterials_, &xxx->pMesh_);
	if (hr != D3D_OK) {
		MessageBox(NULL, _TEXT("Xファイルの読み込みに失敗"), fname, MB_OK);
		return 0;
	}

	// マテリアル情報とテクスチャ情報を取得し、メインメモリに保存しておく。
	// マテリアル数＝テクスチャ数という仮定ができるので、numMaterials_の
	// 数だけ配列を作成する。
	xxx->pMaterials_ = new D3DMATERIAL9[xxx->numMaterials_];
	xxx->pTextures_ = new IDirect3DTexture9* [xxx->numMaterials_];

	// マテリアル情報にアクセスするため、ポインタを得る。
	// ポインタは汎用型なので、マテリアル型に変換しなければならない。
	D3DXMATERIAL *mat = (D3DXMATERIAL *)pMB->GetBufferPointer();

	// マテリアルは複数個ありうるので、その全てについて、情報を取得
	for(unsigned int ii = 0; ii < xxx->numMaterials_; ii++) {
		// MatD3Dはマテリアル（色）情報が格納されている構造体。
		xxx->pMaterials_[ii] = mat[ii].MatD3D;
		xxx->pTextures_[ii] = NULL;
		if (mat[ii].pTextureFilename != NULL) {
			// テクスチャデータを読み込む。
			TCHAR file_path[FILENAME_MAX] = { 0 };
#if defined(UNICODE)
			TCHAR tmp[FILENAME_MAX] = { 0 };
			int len = (int)strlen(mat[ii].pTextureFilename);
			assert(0 < len && len < FILENAME_MAX);
			if (0 < len && len < FILENAME_MAX) {
				MultiByteToWideChar(CP_ACP, MB_COMPOSITE, mat[ii].pTextureFilename, len, tmp, _countof(tmp));
			}
			_stprintf_s(file_path, _countof(file_path), _TEXT("data/%s"), tmp);
#else
			sprintf_s(file_path, "data/%s", mat[ii].pTextureFilename);
#endif
			hr = D3DXCreateTextureFromFile(data->dev, file_path, &(xxx->pTextures_[ii]));
			// 読み込み失敗した場合はテクスチャ無し
			if (hr != D3D_OK) {
				xxx->pTextures_[ii] = NULL;
			}
		}
	}
	// マテリアル情報を解放
	pMB->Release();

	return xxx;
}

// Xデータを描画する
void MyXData::draw(DataSet *data) {
	for(unsigned int ii = 0; ii < this->numMaterials_; ii++) {
		// テクスチャがあれば、設定する
		data->dev->SetTexture(0, this->pTextures_[ii]);
		// マテリアル情報を設定し、ポリゴンに色をつける
		data->dev->SetMaterial(&(this->pMaterials_[ii]));

		// ポリゴンメッシュを描画
		this->pMesh_->DrawSubset(ii);
	}
}


////////////////////////// class MyBall //////////////////////////
// ボールの位置や速度を保持するクラス
class MyBall {
private:
	float vx, vy;		// 速度
	float xx, yy;		// 位置
	MyXData *xdata;		// Xファイルデータ
	MyBall *target[10];	// 衝突相手となるボール
	TCHAR name[4];		// 名前、デバッグ用

public:
	//コンストラクタの宣言(後に実装する).
	MyBall(DataSet *data, const TCHAR fname[], const TCHAR nn[]);
	~MyBall() {
		if (xdata) delete xdata;
	}
	void move(float x, float y) { xx = x; yy = y; }
	void vel(float x, float y) { vx = x; vy = y; }
	bool update(DataSet *data, float tt, int cc);
	void draw(DataSet *data);
	void setTarget(MyBall *bb);
	void getPos(float &x, float &y) { x = xx; y = yy; }
	void getVel(float &x, float &y) { x = vx; y = vy; }
	float energy() { return vx*vx+vy*vy; }//運動エネルギーの計算.

	void friction() { vx *= 0.9998; vy *= 0.9998; }

};
//ここでコンストラクタが実装される.
MyBall::MyBall(DataSet *data, const TCHAR fname[], const TCHAR nn[]) {
	_tcscpy_s(name, _countof(name), nn);//文字列を安全にコピーするための関数.
	xdata = MyXData::load(data, fname);
	vx = vy = 0;
	xx = yy = 0;
	for(int ii = 0; ii < 10; ii++) {
		target[ii] = NULL;
	}
}

// (x1, y1) から (x2, y2) までの距離の２乗を求める(反射ベクトルを求める際に使用).
float distance2(float x1, float y1, float x2, float y2) {
	return (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2);
}

// 内積を求める.
float innerProd(float aa, float bb, float cc, float dd) {
	return aa*cc + bb*dd;
}

// 位置A(xx,yy)、B(xx2,yy2)からなるベクトルABの単位ベクトル(rx,ry)を求める.
void unitVector(float xx, float yy, float xx2, float yy2, float& rx, float& ry) {
	float aa = xx - xx2;
	float bb = yy - yy2;
	float cc = (float)sqrt(aa*aa + bb*bb);//ピタゴラスの定理.
	rx = aa / cc;					//cos.
	ry = bb / cc;					//sin.
}

// 他のボール(target)を考慮しながら、移動計算をする.
bool MyBall::update(DataSet *data, float tt, int cc) {
	// だんだん速度が落ちるように係数qqを設定.
	//const float qq = 0.99999f;
	const float qq = 0.999996f;
	// 速度を落とし、移動位置を求める.
	vx *= qq;  vy *= qq;
	float px = xx + vx * tt;//これから移動するであろうX座標(これからオブジェクトに触れないか検証).
	float py = yy + vy * tt;//これから移動するであろうX座標.
	
	//ここで衝突計算を行う
	//衝突があるかどうかを確認し、あれば速度を交換する.
	for(int ii = 0; ii < 10; ii++)
	{
	    //実体がないなら処理を行わない.
		if (target[ii] == nullptr) continue;


		//target[ii]に実体あり。ターゲットまでの距離を計算.
		float tx, ty;
		target[ii]->getPos(tx, ty);	//ターゲットの位置を取得.
		float dist = distance2(px, py, tx, ty);//自身とターゲットの二点間の距離を測る.
		if (dist < 4) {//distが規定の値より小さいなら触れている.
			
			//衝突のための計算を行う。現在の２ボールの位置からST軸ベクトル計算.
			//T軸(vecAxisTx, vecAxisTy), S軸(vecAxisSx,vecAxisSy)==(-vecAxisTy,vecAxisTx).
			float vecAxisTx, vecAxisTy;		//「Tベクトル」を格納する変数.

			//単位ベクトルは大きさが1だけど、方向を表すことができるベクトル.
			unitVector(px, py, tx, ty, vecAxisTx, vecAxisTy);//単位ベクトルの作成.
	
			//S軸は(-Ty,Tx)
			float vecAxisSx = -vecAxisTy;	//unitVectorで求めたTベクトルを90度回転してS軸ベクトルを（x,y座標系で）求める.
			float vecAxisSy = vecAxisTx;
			
			//相手の速度ベクトルを取得.
			float tvx, tvy;
			target[ii]->getVel(tvx, tvy);

			//係数を計算.

			//自身の速度を Va とする.
			float VaT = innerProd(vx, vy, vecAxisTx, vecAxisTy);    //自分の速度ベクトルのT成分.
			float VaS = innerProd(vx, vy, vecAxisSx, vecAxisSy);	//自分の速度ベクトルのS成分.
			//相手の速度を Vb とする.
			float VbT = innerProd(tvx, tvy, vecAxisTx, vecAxisTy);	//相手の速度ベクトルのT成分.
			float VbS = innerProd(tvx, tvy, vecAxisSx, vecAxisSy);	//相手の速度ベクトルのS成分.

			//衝突後、速度のT成分を交換した速度をx,y座標成分に戻す.
			{//一つの処理としてグループ化.
			//自分の速度	Va’= (Va･S)S + (Vb･T)T.

				printf("\n");
				printf("自身の速度v=(%.2g,%.2g)->", vx, vy);

				vx = innerProd(VaS, VbT, vecAxisSx, vecAxisTx);	//vx = VaS * vecAxisSx + VbT * vecAxisTx;
				vy = innerProd(VaS, VbT, vecAxisSy, vecAxisTy);	//vy = VaS * vecAxisSy + VbT * vecAxisTy;

				printf("\n");
				printf("自身の新しい速度vx = %.2g,vy = %.2g ", vx, vy);

				//相手の速度	Vb’= (Vb･S)S + (Va･T)T.

				printf("\n");
				printf("相手の速度tv=(%.2g,%.2g)->", tvx, tvy);


				tvx = innerProd(VbS, VaT, vecAxisSx, vecAxisTx);//tvx = VbS * vecAxisSx + Vat * vecAxisTx
				tvy = innerProd(VbS, VaT, vecAxisSy, vecAxisTy);//tvy = VbS * vecAxisSy + Vat * vecAxisTy
				
				printf("\n");
				printf("相手の新しい速度tvx = %.2g,tvy = %.2g ", tvx, tvy);

				target[ii]->vel(tvx, tvy);
			}
			

			printf("\n");
		}
	}

	//最終結果を保存
	xx = px;
	yy = py;

	//壁反射を調べる。反射すると速度がやや落ちる.
	//const float mm = -0.9f;
	const float mm = -0.98f;
	if (xx <= BOARD_LEFT && vx < 0) { vx *= mm; }
	if (xx >= BOARD_RIGHT && vx > 0) { vx *= mm; }
	if (yy <= BOARD_TOP && vy < 0) { vy *= mm; }
	if (yy >= BOARD_BOTTOM && vy > 0) { vy *= mm; }
	
	
	if (energy() < 0.0001f) {
		vx = vy = 0;
		return false;
	}
	return true;
}

// 現在位置をワールド行列に設定して、描画.
void MyBall::draw(DataSet *data) {
	D3DXMATRIX mat;
	D3DXMatrixTranslation(&mat, xx, yy, 1);
	data->dev->SetTransform(D3DTS_WORLD, &mat);
	if (xdata) xdata->draw(data);
}

// 自分が衝突しうる相手を登録する.
void MyBall::setTarget(MyBall *bb) {
	//0から9のいずれかに相手の情報を保存する(10個以上の対象があるならii<10の値を変更しなければならない).
	for(int ii = 0; ii < 10; ii++) {
		if (target[ii] == NULL) {
			target[ii] = bb;
			break;
		}
	}
}

////////////////////////// ::resetData //////////////////////////
void resetData(DataSet *data) {
}

////////////////////////// ::setCamera //////////////////////////
// カメラを設定する。カメラの位置、向きの他、スクリーン情報もある.
void setCamera(DataSet *data) {
	HRESULT hr = D3D_OK;
	// ビュー行列作成のため、カメラ位置を決める.
	// カメラは自分の位置、見ている点（注視点）、姿勢の３要素からなる.
	D3DXVECTOR3 cam(0, 0, data->cam_transz);	// カメラ位置は (0, 0, transz).
	D3DXVECTOR3 camAt(0, 0, 0); // カメラの注視点は原点.
	D3DXVECTOR3 camUp(0, 1, 0); // カメラは垂直姿勢 up=(0, 1, 0).

	// カメラ位置情報から行列を作成。RHは右手系の意味.
	D3DXMATRIX camera;
	D3DXMatrixLookAtRH(&camera, &cam, &camAt, &camUp);

	// カメラを回転させる。SHIFTキー＋マウス左ボタン.
	D3DXMATRIX mx, my;
	// X軸とY軸中心の回転を行列で表現.
	D3DXMatrixRotationZ(&my, data->cam_roty);
	D3DXMatrixRotationX(&mx, data->cam_rotx);
	// camera行列とかけ算.
	camera = my * mx * camera;
	// かけ算の順番を変えると表示が変わる.
	//camera = mx * my * camera;

	// ビュー行列として設定.
	data->dev->SetTransform(D3DTS_VIEW, &camera);

	// 投影行列を設定する。スクリーンに投影するために必要。視野角はπ/3.
	D3DXMATRIX proj;
	D3DXMatrixPerspectiveFovRH(&proj, D3DX_PI/3, data->aspect, 0.1f, 200.0f);
	// 作成した行列を、投影行列に設定.
	data->dev->SetTransform(D3DTS_PROJECTION, &proj);
}

////////////////////////// ::setLight //////////////////////////
// ライト情報を設定する。三灯照明を用いる.
void setLight(DataSet *data) {
	// ライトの光量と位置を決め、0番のライトとして設定、有効にする.
	D3DLIGHT9 light;
	ZeroMemory(&light, sizeof(D3DLIGHT9));
	light.Type = D3DLIGHT_DIRECTIONAL;		// ディレクショナルライト.

	// キーライトは強い白色に.
	light.Diffuse.r = 1.0;
	light.Diffuse.g = 1.0;
	light.Diffuse.b = 1.0;
	light.Ambient.r = 0.2f;
	light.Ambient.g = 0.2f;
	light.Ambient.b = 0.2f;
	// 方向は(1,-1,-1)を向くライト.
	light.Direction.x = 1;
	light.Direction.y = -1;
	light.Direction.z = -1;
	data->dev->SetLight(0, &light);		// 0番に設定.
	data->dev->LightEnable(0, data->light[0]);	// 有効にする.

	// フィルライトは弱い白色に.
	light.Diffuse.r = 0.6f;
	light.Diffuse.g = 0.6f;
	light.Diffuse.b = 0.6f;
	// 方向は(-1, 1,-1)を向くライト。０番ライトと左右に逆方向.
	light.Direction.x = -1;
	light.Direction.y = 1;
	light.Direction.z = -1;
	data->dev->SetLight(1, &light);		// 1番に設定.
	data->dev->LightEnable(1, data->light[1]);	// 有効にする.

	// バックライトは強めに.
	light.Diffuse.r = 0.8f;
	light.Diffuse.g = 0.8f;
	light.Diffuse.b = 0.8f;
	// 方向は(-1, 1,1)を向くライト。モデルを後ろから照らす.
	light.Direction.x = -1;
	light.Direction.y = -1;
	light.Direction.z = 1;
	data->dev->SetLight(2, &light);		// 2番に設定.
	data->dev->LightEnable(2, data->light[2]);	// 有効にする.
}

////////////////////////// ::updateData //////////////////////////
// データをアップデートする.
// param data データセット.
void updateData(DataSet *data) {
	data->gameTimer++;		// ゲームタイマーを進行.

	// 少しずつ進めて、お互いの衝突を考慮させる.
	// divideが大きいほど計算単位が短くなる.
	const int divide = 10;
	for(int ii = 0; ii < divide; ii++) {
		for(int bb = 0; data->ball[bb]; bb++) {

			data->ball[bb]->friction();
			data->ball[bb]->update(data, 1.0f/divide, ii);
		}
	}
}


////////////////////////// ::drawData //////////////////////////
// DataSetに基づいて、スプライトを描画.
// param data データセット.
// return 発生したエラー.
HRESULT drawData(DataSet *data) {
	HRESULT hr = D3D_OK;

	// 背景色を決める。RGB=(200,200,255)とする.
	D3DCOLOR rgb = D3DCOLOR_XRGB(200, 200, 250);
	// 画面全体を消去.
	data->dev->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, rgb, 1.0f, 0);

	// 描画を開始（シーン描画の開始）.
	data->dev->BeginScene();

	// カメラ情報を設定する.
	setCamera(data);
	// ライト情報を設定する.
	setLight(data);

	// デフォルトはライティングOFF.
	data->dev->SetRenderState(D3DRS_LIGHTING, data->light[3]);
	if (data->flag[0] == false) {
		data->dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	} else {
		data->dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	}

	// ワールド座標系の設定はまず単位行列を.
	D3DXMATRIX ww;
	D3DXMatrixIdentity(&ww);
	data->dev->SetTransform(D3DTS_WORLD, &ww);

	// Xデータの描画.
	data->raxa->draw(data);
	for(int bb = 0; data->ball[bb]; bb++) {
		data->ball[bb]->draw(data);
	}

	data->dev->EndScene();

	// 実際に画面に表示。バックバッファからフロントバッファへの転送.
	// デバイス生成時のフラグ指定により、ここでVSYNCを待つ.
	data->dev->Present(NULL, NULL, NULL, NULL);

	float totalEnergy = 0;
	for(int bb = 0; data->ball[bb]; bb++) {
		totalEnergy += data->ball[bb]->energy();
	}

	// Direct3D表示後、GDIにより文字列表示.
	HDC hdc = GetDC(data->hWnd);
	SetTextColor(hdc, RGB(0xFF, 0x00, 0x00));
	SetBkMode(hdc, TRANSPARENT);
	TCHAR text[256] = {0};
	_stprintf_s(text, _TEXT("LIGHT %s %d%d%d, cam rot=(%.2f, %.2f) z=%.2f flag %d%d%d, ee=%.5g"), 
		data->light[3]?_TEXT("ON"):_TEXT("OFF"), data->light[0], data->light[1], data->light[2], 
		data->cam_rotx, data->cam_roty, data->cam_transz, 
		data->flag[0], data->flag[1], data->flag[2], totalEnergy);
	TextOut(hdc, 20, 700, text, (int)_tcsclen(text));		// 文字列の表示.
	ReleaseDC(data->hWnd, hdc);

	return D3D_OK;
}

// イベント処理コールバック（ウィンドウプロシージャ）.
// イベント発生時にDispatchMessage関数から呼ばれる.
// param hWnd イベントの発生したウィンドウ.
// param uMsg イベントの種類を表すID.
// param wParam 第一メッセージパラメータ.
// param lParam 第二メッセージパラメータ.
// return DefWindowProcの戻り値に従う.
//
LRESULT CALLBACK
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// マウス位置記録用変数.
	static int mxx, myy;	
	// イベントの種類に応じて、switch文にて処理を切り分ける.
	switch(uMsg) {
	case WM_KEYDOWN:
		// ESCキーが押下されたら終了.
		if (wParam == VK_ESCAPE) PostQuitMessage(0);
		if (wParam == '1') mydata.light[0] = !mydata.light[0];
		if (wParam == '2') mydata.light[1] = !mydata.light[1];
		if (wParam == '3') mydata.light[2] = !mydata.light[2];
		if (wParam == '4') mydata.light[3] = !mydata.light[3];
		if (wParam == 'W') mydata.flag[0] = !mydata.flag[0];
		if (wParam == ' ') mydata.flag[1] = !mydata.flag[1];
		if (wParam == 'Z') mydata.flag[2] = TRUE;
		if (wParam == 'R') resetData(&mydata);
		//if (wParam == 'M') data->ball[0]->vel(1.4f, 0.3f);
		break;
	case WM_SIZE:
		mydata.aspect = LOWORD(lParam) / (float)HIWORD(lParam);
		break;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		// ボタンを押されたら位置を記録しておく.
		mxx = MAKEPOINTS(lParam).x;
		myy = MAKEPOINTS(lParam).y;
		break;

	case WM_MOUSEMOVE: {
		int xx = MAKEPOINTS(lParam).x;
		int yy = MAKEPOINTS(lParam).y;
		// 左ボタン押下で、カメラ回転に.
		if (wParam & MK_LBUTTON) {
			if (wParam) {
				mydata.cam_roty -= (mxx - xx) * 0.01f;
				mydata.cam_rotx -= (myy - yy) * 0.01f;
			}
		}
		// 右ボタン押下で、カメラの前後移動に.
		if (wParam & MK_RBUTTON) {
			mydata.cam_transz += (myy - yy) * 0.1f;
		}
		mxx = xx; myy = yy;
	}
		break;

	case WM_CLOSE:		// 終了通知(CLOSEボタンが押された場合など)が届いた場合.
		// プログラムを終了させるため、イベントループに0を通知する.
		// この結果、GetMessageの戻り値は0になる.
		PostQuitMessage(0);
		break;
	default:
		break;
	}

	// デフォルトのウィンドウイベント処理.
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Windowを作成する.
// return ウィンドウハンドル.
//
HWND initWindow(DataSet *data) {
	// まずウィンドウクラスを登録する.
	// これはウィンドウ生成後の処理の仕方をWindowsに教えるためである.
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));	// 変数wcをゼロクリアする.
	wc.cbSize = sizeof(WNDCLASSEX);			// この構造体の大きさを与える.
	wc.lpfnWndProc = (WNDPROC)WindowProc;	// ウィンドウプロシージャ登録.
	wc.hInstance = data->hInstance;				// インスタンスハンドルを設定.
	wc.hCursor = LoadCursor(NULL, IDC_CROSS);	// マウスカーソルの登録.
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);	// 背景をGRAYに.
	wc.lpszClassName = wndClsName;			// クラス名、CreateWindowと一致させる.
	RegisterClassEx(&wc);			// 登録.

	// ウィンドウを作成する。クラス名は"directx".
	data->hWnd = CreateWindow(wndClsName, version, WS_OVERLAPPEDWINDOW, 
		0, 0, WIDTH, HEIGHT, NULL, NULL, data->hInstance, NULL);

	return data->hWnd;
}

// テクスチャfnameを読み込む。エラーが発生したら例外を発生する.
// param fname ファイル名。const修飾子をつけ関数内で値を変更しないことを宣言する.
// param tex 作成するテクスチャへのポインタを入れるためのポインタ.
HRESULT loadTexture(IDirect3DDevice9 *dev, const TCHAR fname[], IDirect3DTexture9 **tex) {
	HRESULT hr = D3DXCreateTextureFromFile(dev, fname, tex);
	if ( FAILED(hr) ) {
		MessageBox(NULL, _TEXT("テクスチャ読み込み失敗"), fname, MB_OK);
		throw hr;		// エラーが発生したので例外を送る.
	}
	return hr;
}

// メインループ.
// param hInstance アプリケーションを表すハンドル.
void MainLoop(DataSet *data) {
	HRESULT hr = E_FAIL;
	data->hWnd = initWindow(data);			// ウィンドウを作成する.

	// Direct3Dを初期化する
	hr = initDirect3D(data);
	if ( FAILED(hr) ) {
		MessageBox(NULL, d3dErrStr(hr), _TEXT("Direct3D初期化失敗"), MB_OK);
		return;
	}
	// 各種3D描画設定を行う.
	data->dev->SetRenderState(D3DRS_ZENABLE, TRUE);		// Zバッファリング有効.
	// ライトON.
	data->light[0] = data->light[1] = data->light[2] = TRUE;
	data->light[3] = TRUE;
	data->aspect = (float)WIDTH / (float)HEIGHT;
	data->cam_transz = 50;

	// raxa.x, ball.xを読み込む.
	data->raxa = MyXData::load(data, _TEXT("data/raxa.x"));
  
	
	//キューボール.
	data->ball[0] = new MyBall(data, _TEXT("data/ballB.x"), _TEXT("B"));
	data->ball[0]->move(-2.0f, 0.0f);//初期位置.
	data->ball[0]->vel(0.7f, 0.01f);//初期速度.
	
	data->ball[1] = new MyBall(data, _TEXT("data/ball1.x"), _TEXT("1"));
	data->ball[1]->move(5.0f, 0.0f);
	data->ball[1]->vel(0.0f, 0.0f);

	data->ball[2] = new MyBall(data, _TEXT("data/ball2.x"), _TEXT("2"));
	data->ball[2]->move(12.5f, 0.0f);
	data->ball[2]->vel(0.0f, 0.0f);
	
	data->ball[3] = new MyBall(data, _TEXT("data/ball3.x"), _TEXT("3"));
	data->ball[3]->move(7.0f, -1.0f);
	data->ball[3]->vel(0.0f, 0.0f);
	
	data->ball[4] = new MyBall(data, _TEXT("data/ball4.x"), _TEXT("4"));
	data->ball[4]->move(7.0f, 1.0f);
	data->ball[4]->vel(0.0f, 0.0f);
	
	data->ball[5] = new MyBall(data, _TEXT("data/ball5.x"), _TEXT("5"));
	data->ball[5]->move(8.75f, 0.0f);
	data->ball[5]->vel(0.0f, 0.0f);

	data->ball[6] = new MyBall(data, _TEXT("data/ball6.x"), _TEXT("6"));
	data->ball[6]->move(8.75f, 2.0f);
	data->ball[6]->vel(0.0f, 0.0f);

	data->ball[7] = new MyBall(data, _TEXT("data/ball7.x"), _TEXT("7"));
	data->ball[7]->move(8.75f, -2.0f);
	data->ball[7]->vel(0.0f, 0.0f);

	data->ball[8] = new MyBall(data, _TEXT("data/ball8.x"), _TEXT("8"));
	data->ball[8]->move(10.5f, 1.0f);
	data->ball[8]->vel(0.0f, 0.0f);

	data->ball[9] = new MyBall(data, _TEXT("data/ball9.x"), _TEXT("9"));
	data->ball[9]->move(10.5f, -1.0f);
	data->ball[9]->vel(0.0f, 0.0f);


	// 衝突対象を登録
	for (int i = 0; i < 10; i++) {

		if (data->ball[i] == nullptr)
		{
			continue; // i番目がnullならスキップ.
		}
		for (int j = 0; j < 10; j++) {
            //同じ番号かnullならスキップ.
			if (i == j || 
				data->ball[j] == nullptr)
			{
				continue;
			}
			data->ball[i]->setTarget(data->ball[j]);
		}
	}

	data->flag[1] = false;		// 再生OFF.


	ShowWindow(data->hWnd, SW_SHOWNORMAL);	// 作成したウィンドウを表示する.

	// 反時計回りの頂点を持つ背面をカリング.
	//data->dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	// カリングを無効に.
	data->dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	// イベントループ.
	// ブロック型関数GetMessageではなくノンブロック型関数のPeekMessageを使う.
	MSG msg;
	bool flag = 1;
	while(flag) {
		// メッセージ(入力)があるかどうか確認する.
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			// メッセージがあるので処理する.

			//msgにデータを格納.
			if (GetMessage(&msg, NULL, 0, 0) == 1) {
				TranslateMessage(&msg);//入力イベントを適切に処理できるよう修正.
				DispatchMessage(&msg);//ウィンドウプロシージャに送信.
			} else {
				flag = 0;
			}
		}
		else {//入力による処理と描画処理は同時には行われない.
			if (data->flag[1] || data->flag[2]) {
				data->flag[2] = FALSE;
				updateData(data);	// 位置の再計算.

				
			}
			drawData(data);		// 描画.
		}
	}
}

#define RELEASE(__xx__) if (__xx__) { __xx__->Release(); __xx__ = 0; }

// DataSetを解放する.
void ReleaseDataSet(DataSet *data) {

	for (int i = _countof(data->ball) - 1; i >= 0; i-- ) {
		if (data->ball[i]) {
			delete data->ball[i];
		}
	}
	if (data->raxa) {
		delete data->raxa;
	}

	RELEASE(data->dev);
	RELEASE(data->pD3D);
}

// param argc コマンドラインから渡された引数の数.
// param argv 引数の実体へのポインタ配列.
// return 0 正常終了.
int main(int argc, char *argv[]) {
	srand((UINT)time(NULL));// 乱数系列の初期化.

	// このプログラムが実行されるときのインスタンスハンドルを取得.
	mydata.hInstance = GetModuleHandle(NULL);
	MainLoop(&mydata);
	
	// 確保したリソースを解放.
	ReleaseDataSet(&mydata);
	return 0;
}

// Direct3Dを初期化する.
// param data データセット.
// return 発生したエラーまたはD3D_OK.
//
HRESULT initDirect3D(DataSet *data) {
	HRESULT hr;

	// Direct3Dインスタンスオブジェクトを生成する.
	// D3D_SDK_VERSIONと、ランタイムバージョン番号が適切でないと、NULLが返る.
	data->pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	// NULLならランタイムが不適切.
	if (data->pD3D == NULL) return E_FAIL;

	// PRESENTパラメータをゼロクリアし、適切に初期化.
	ZeroMemory(&(data->d3dpp), sizeof(data->d3dpp));
	// ウィンドウモードに.
	data->d3dpp.Windowed = TRUE;
	data->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	// バックバッファはRGBそれぞれ８ビットで.
	data->d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	// Present時に垂直同期に合わせる.
	data->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

	// 3D用にZバッファ(depth buffer)を作成.
	data->d3dpp.EnableAutoDepthStencil = TRUE;
	data->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	// D3Dデバイスオブジェクトの作成.
	hr = data->pD3D ->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, data->hWnd, 
		D3DCREATE_HARDWARE_VERTEXPROCESSING, &(data->d3dpp), &(data->dev));
	if (FAILED(hr)) {
		// D3Dデバイスオブジェクトの作成。HAL&SOFT.
		hr = data->pD3D ->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, data->hWnd, 
			D3DCREATE_SOFTWARE_VERTEXPROCESSING, &(data->d3dpp), &(data->dev));
		if (FAILED(hr)) {
			// D3Dデバイスオブジェクトの作成。REF&SOFT.
			hr = data->pD3D ->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, data->hWnd, 
				D3DCREATE_SOFTWARE_VERTEXPROCESSING, &(data->d3dpp), &(data->dev));
		}
	}
	if ( FAILED(hr) ) return hr;

	return D3D_OK;
}

// エラー発生時のHRESULTを文字列に変換するための補助関数.
const TCHAR *d3dErrStr(HRESULT res) {
	switch(res) {
	case D3D_OK: return _TEXT("D3D_OK");
	case D3DERR_DEVICELOST: return _TEXT("D3DERR_DEVICELOST");
	case D3DERR_DRIVERINTERNALERROR: return _TEXT("D3DERR_DRIVERINTERNALERROR");
	case D3DERR_INVALIDCALL: return _TEXT("D3DERR_INVALIDCALL");
	case D3DERR_OUTOFVIDEOMEMORY: return _TEXT("D3DERR_OUTOFVIDEOMEMORY");
	case D3DERR_DEVICENOTRESET: return _TEXT("D3DERR_DEVICENOTRESET");
	case D3DERR_NOTAVAILABLE: return _TEXT("D3DERR_NOTAVAILABLE");
	case D3DXERR_INVALIDDATA: return _TEXT("D3DXERR_INVALIDDATA");
	case E_OUTOFMEMORY: return _TEXT("E_OUTOFMEMORY");
	}
	return _TEXT("unknown error");
}
