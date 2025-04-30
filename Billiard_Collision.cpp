#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <d3d9.h>			// Direct3D9 (DirectX9 Graphics)�ɕK�v
#include <d3dx9core.h>		// DirectX �X�v���C�g�ɕK�v
#include <tchar.h>
#include <time.h>			// srand�̎�Ɍ��ݎ������g������.
#include <assert.h>

using namespace std;

// �E�B���h�E�̕��ƍ�����萔�Őݒ肷��
#define WIDTH 1024			//< ��
#define HEIGHT 768			//< ����

// �r�����[�h��̊e�ӂ̈ʒu(�ǂɐG�ꂽ�Ƃ��A�ǂ̖ʂɐG�ꂽ���̎Q�ƂɎg�p�H)
#define BOARD_TOP -18
#define BOARD_BOTTOM 18
#define BOARD_LEFT -35
#define BOARD_RIGHT 35


// �E�B���h�E�^�C�g���o�[�ɕ\�������o�[�W������
static const TCHAR version[] = _TEXT(__DATE__ " " __TIME__);

// �E�B���h�E�N���X��.
static const TCHAR wndClsName[] = _TEXT("3D Billiards");

// �N���X�錾�B���̂̒�`����ɃN���X��������錾���Ă����B
struct DataSet;
class MyXData;
class MyBall;

// �֐��錾
HRESULT initDirect3D(DataSet *data);
const TCHAR *d3dErrStr(HRESULT res);
void ReleaseDataSet(DataSet *data);

// �v���O�����ɕK�v�ȕϐ����\���̂Ƃ��Ē�`
struct DataSet {
	HINSTANCE hInstance;		//< �C���X�^���X�n���h��
	HWND hWnd;					//< �\���E�B���h�E
	IDirect3D9 *pD3D;			//< Direct3D�C���X�^���X�I�u�W�F�N�g
	D3DPRESENT_PARAMETERS d3dpp;//< �f�o�C�X�쐬���̃p�����[�^
	IDirect3DDevice9 *dev;		//< Direct3D�f�o�C�X�I�u�W�F�N�g
	int gameTimer;				//< �S�̗p�^�C�}�[

	// 3D�\���p�̃f�[�^
	float aspect;				//< ��ʃA�X�y�N�g��i�c����j
	float cam_rotx;				//< �J������]��
	float cam_roty;
	float cam_transz;			//< �J������Z�ʒu

	// ���C�g��ON/OFF
	bool light[4];				//< ���C�g��ON/OFF�p�t���O
	// �e��t���O
	bool flag[10];				//< 0:SOLID/WIRE

	MyBall *ball[11];			//< �{�[���z��B�ԕ���u���̂�10+1��
	MyXData *raxa;				// �r�����[�h���X�f�[�^
} mydata;

////////////////////////// class MyXData //////////////////////////
// X�f�[�^���Ǘ�����N���X�B
// �����o�֐���load�����邪�A���̊֐���static�֐��B�܂�A�N���X�֐��B
// �C���X�^���X�𐶐����邽�߂̊֐��ƂȂ��Ă���B
class MyXData {
private:
	// X�t�@�C���ǂݍ��݂̂��߂ɕK�v�ȕϐ�
	ID3DXMesh *pMesh_;				//< �`��f�[�^
	DWORD numMaterials_;			//< �}�e���A���f�[�^��
	IDirect3DTexture9 **pTextures_;	//< �e�N�X�`�����
	D3DMATERIAL9 *pMaterials_;		//< �}�e���A���f�[�^

public:
	MyXData() :pMesh_(0), numMaterials_(0), pTextures_(0), pMaterials_(0) {}
	~MyXData();
	void draw(DataSet *data);		//< �`��

	// static�����o�֐��́A�N���X�֐��ƂȂ�B
	static MyXData *load(DataSet *data, const TCHAR *fname);
};

// �f�X�g���N�^�ł͑S�f�[�^���������
MyXData::~MyXData() {
	if (pMesh_) pMesh_->Release();
	for(unsigned int ii = 0; ii < numMaterials_; ii++) {
		if (pTextures_[ii]) pTextures_[ii]->Release();
	}
	if (pTextures_) delete[] pTextures_;
	if (pMaterials_) delete[] pMaterials_;
}


// X�t�@�C����ǂݍ��ށB�������g�𐶐����邽�߁Astatic�����o�֐��ƂȂ��Ă���B
// arg data �f�[�^�Z�b�g
// arg fname �t�@�C����
MyXData *MyXData::load(DataSet *data, const TCHAR *fname) {
	MyXData *xxx = new MyXData();		// �C���X�^���X�𐶐�

	// �w�肷��X�t�@�C����ǂݍ���ŁA�V�X�e���������ɓW�J����B
	// �|���S�����b�V�����̑��ɁA�}�e���A�������ǂݍ��ށB
	HRESULT hr;
	ID3DXBuffer *pMB;		// �}�e���A�����̂��߂̃f�[�^�o�b�t�@

	// D3DXLoadMeshFromX�ɂ��X�t�@�C������|���S�����b�V����ǂݍ��ށB
	// �f�[�^�z�u�ꏊ�Ƃ���SYSTEMMEM(�V�X�e��������)��I������B
	// ����ɂ��t���X�N���[���؂�ւ����ɂ��f�[�^�͔j�󂳂�Ȃ��B
	hr = D3DXLoadMeshFromX(fname, D3DXMESH_SYSTEMMEM, data->dev, NULL, 
		&pMB, NULL, &xxx->numMaterials_, &xxx->pMesh_);
	if (hr != D3D_OK) {
		MessageBox(NULL, _TEXT("X�t�@�C���̓ǂݍ��݂Ɏ��s"), fname, MB_OK);
		return 0;
	}

	// �}�e���A�����ƃe�N�X�`�������擾���A���C���������ɕۑ����Ă����B
	// �}�e���A�������e�N�X�`�����Ƃ������肪�ł���̂ŁAnumMaterials_��
	// �������z����쐬����B
	xxx->pMaterials_ = new D3DMATERIAL9[xxx->numMaterials_];
	xxx->pTextures_ = new IDirect3DTexture9* [xxx->numMaterials_];

	// �}�e���A�����ɃA�N�Z�X���邽�߁A�|�C���^�𓾂�B
	// �|�C���^�͔ėp�^�Ȃ̂ŁA�}�e���A���^�ɕϊ����Ȃ���΂Ȃ�Ȃ��B
	D3DXMATERIAL *mat = (D3DXMATERIAL *)pMB->GetBufferPointer();

	// �}�e���A���͕������肤��̂ŁA���̑S�Ăɂ��āA�����擾
	for(unsigned int ii = 0; ii < xxx->numMaterials_; ii++) {
		// MatD3D�̓}�e���A���i�F�j��񂪊i�[����Ă���\���́B
		xxx->pMaterials_[ii] = mat[ii].MatD3D;
		xxx->pTextures_[ii] = NULL;
		if (mat[ii].pTextureFilename != NULL) {
			// �e�N�X�`���f�[�^��ǂݍ��ށB
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
			// �ǂݍ��ݎ��s�����ꍇ�̓e�N�X�`������
			if (hr != D3D_OK) {
				xxx->pTextures_[ii] = NULL;
			}
		}
	}
	// �}�e���A���������
	pMB->Release();

	return xxx;
}

// X�f�[�^��`�悷��
void MyXData::draw(DataSet *data) {
	for(unsigned int ii = 0; ii < this->numMaterials_; ii++) {
		// �e�N�X�`��������΁A�ݒ肷��
		data->dev->SetTexture(0, this->pTextures_[ii]);
		// �}�e���A������ݒ肵�A�|���S���ɐF������
		data->dev->SetMaterial(&(this->pMaterials_[ii]));

		// �|���S�����b�V����`��
		this->pMesh_->DrawSubset(ii);
	}
}


////////////////////////// class MyBall //////////////////////////
// �{�[���̈ʒu�⑬�x��ێ�����N���X
class MyBall {
private:
	float vx, vy;		// ���x
	float xx, yy;		// �ʒu
	MyXData *xdata;		// X�t�@�C���f�[�^
	MyBall *target[10];	// �Փˑ���ƂȂ�{�[��
	TCHAR name[4];		// ���O�A�f�o�b�O�p

public:
	//�R���X�g���N�^�̐錾(��Ɏ�������).
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
	float energy() { return vx*vx+vy*vy; }//�^���G�l���M�[�̌v�Z.

	void friction() { vx *= 0.9998; vy *= 0.9998; }

};
//�����ŃR���X�g���N�^�����������.
MyBall::MyBall(DataSet *data, const TCHAR fname[], const TCHAR nn[]) {
	_tcscpy_s(name, _countof(name), nn);//����������S�ɃR�s�[���邽�߂̊֐�.
	xdata = MyXData::load(data, fname);
	vx = vy = 0;
	xx = yy = 0;
	for(int ii = 0; ii < 10; ii++) {
		target[ii] = NULL;
	}
}

// (x1, y1) ���� (x2, y2) �܂ł̋����̂Q������߂�(���˃x�N�g�������߂�ۂɎg�p).
float distance2(float x1, float y1, float x2, float y2) {
	return (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2);
}

// ���ς����߂�.
float innerProd(float aa, float bb, float cc, float dd) {
	return aa*cc + bb*dd;
}

// �ʒuA(xx,yy)�AB(xx2,yy2)����Ȃ�x�N�g��AB�̒P�ʃx�N�g��(rx,ry)�����߂�.
void unitVector(float xx, float yy, float xx2, float yy2, float& rx, float& ry) {
	float aa = xx - xx2;
	float bb = yy - yy2;
	float cc = (float)sqrt(aa*aa + bb*bb);//�s�^�S���X�̒藝.
	rx = aa / cc;					//cos.
	ry = bb / cc;					//sin.
}

// ���̃{�[��(target)���l�����Ȃ���A�ړ��v�Z������.
bool MyBall::update(DataSet *data, float tt, int cc) {
	// ���񂾂񑬓x��������悤�ɌW��qq��ݒ�.
	//const float qq = 0.99999f;
	const float qq = 0.999996f;
	// ���x�𗎂Ƃ��A�ړ��ʒu�����߂�.
	vx *= qq;  vy *= qq;
	float px = xx + vx * tt;//���ꂩ��ړ�����ł��낤X���W(���ꂩ��I�u�W�F�N�g�ɐG��Ȃ�������).
	float py = yy + vy * tt;//���ꂩ��ړ�����ł��낤X���W.
	
	//�����ŏՓˌv�Z���s��
	//�Փ˂����邩�ǂ������m�F���A����Α��x����������.
	for(int ii = 0; ii < 10; ii++)
	{
	    //���̂��Ȃ��Ȃ珈�����s��Ȃ�.
		if (target[ii] == nullptr) continue;


		//target[ii]�Ɏ��̂���B�^�[�Q�b�g�܂ł̋������v�Z.
		float tx, ty;
		target[ii]->getPos(tx, ty);	//�^�[�Q�b�g�̈ʒu���擾.
		float dist = distance2(px, py, tx, ty);//���g�ƃ^�[�Q�b�g�̓�_�Ԃ̋����𑪂�.
		if (dist < 4) {//dist���K��̒l��菬�����Ȃ�G��Ă���.
			
			//�Փ˂̂��߂̌v�Z���s���B���݂̂Q�{�[���̈ʒu����ST���x�N�g���v�Z.
			//T��(vecAxisTx, vecAxisTy), S��(vecAxisSx,vecAxisSy)==(-vecAxisTy,vecAxisTx).
			float vecAxisTx, vecAxisTy;		//�uT�x�N�g���v���i�[����ϐ�.

			//�P�ʃx�N�g���͑傫����1�����ǁA������\�����Ƃ��ł���x�N�g��.
			unitVector(px, py, tx, ty, vecAxisTx, vecAxisTy);//�P�ʃx�N�g���̍쐬.
	
			//S����(-Ty,Tx)
			float vecAxisSx = -vecAxisTy;	//unitVector�ŋ��߂�T�x�N�g����90�x��]����S���x�N�g�����ix,y���W�n�Łj���߂�.
			float vecAxisSy = vecAxisTx;
			
			//����̑��x�x�N�g�����擾.
			float tvx, tvy;
			target[ii]->getVel(tvx, tvy);

			//�W�����v�Z.

			//���g�̑��x�� Va �Ƃ���.
			float VaT = innerProd(vx, vy, vecAxisTx, vecAxisTy);    //�����̑��x�x�N�g����T����.
			float VaS = innerProd(vx, vy, vecAxisSx, vecAxisSy);	//�����̑��x�x�N�g����S����.
			//����̑��x�� Vb �Ƃ���.
			float VbT = innerProd(tvx, tvy, vecAxisTx, vecAxisTy);	//����̑��x�x�N�g����T����.
			float VbS = innerProd(tvx, tvy, vecAxisSx, vecAxisSy);	//����̑��x�x�N�g����S����.

			//�Փˌ�A���x��T�����������������x��x,y���W�����ɖ߂�.
			{//��̏����Ƃ��ăO���[�v��.
			//�����̑��x	Va�f= (Va�S)S + (Vb�T)T.

				printf("\n");
				printf("���g�̑��xv=(%.2g,%.2g)->", vx, vy);

				vx = innerProd(VaS, VbT, vecAxisSx, vecAxisTx);	//vx = VaS * vecAxisSx + VbT * vecAxisTx;
				vy = innerProd(VaS, VbT, vecAxisSy, vecAxisTy);	//vy = VaS * vecAxisSy + VbT * vecAxisTy;

				printf("\n");
				printf("���g�̐V�������xvx = %.2g,vy = %.2g ", vx, vy);

				//����̑��x	Vb�f= (Vb�S)S + (Va�T)T.

				printf("\n");
				printf("����̑��xtv=(%.2g,%.2g)->", tvx, tvy);


				tvx = innerProd(VbS, VaT, vecAxisSx, vecAxisTx);//tvx = VbS * vecAxisSx + Vat * vecAxisTx
				tvy = innerProd(VbS, VaT, vecAxisSy, vecAxisTy);//tvy = VbS * vecAxisSy + Vat * vecAxisTy
				
				printf("\n");
				printf("����̐V�������xtvx = %.2g,tvy = %.2g ", tvx, tvy);

				target[ii]->vel(tvx, tvy);
			}
			

			printf("\n");
		}
	}

	//�ŏI���ʂ�ۑ�
	xx = px;
	yy = py;

	//�ǔ��˂𒲂ׂ�B���˂���Ƒ��x����◎����.
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

// ���݈ʒu�����[���h�s��ɐݒ肵�āA�`��.
void MyBall::draw(DataSet *data) {
	D3DXMATRIX mat;
	D3DXMatrixTranslation(&mat, xx, yy, 1);
	data->dev->SetTransform(D3DTS_WORLD, &mat);
	if (xdata) xdata->draw(data);
}

// �������Փ˂����鑊���o�^����.
void MyBall::setTarget(MyBall *bb) {
	//0����9�̂����ꂩ�ɑ���̏���ۑ�����(10�ȏ�̑Ώۂ�����Ȃ�ii<10�̒l��ύX���Ȃ���΂Ȃ�Ȃ�).
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
// �J������ݒ肷��B�J�����̈ʒu�A�����̑��A�X�N���[����������.
void setCamera(DataSet *data) {
	HRESULT hr = D3D_OK;
	// �r���[�s��쐬�̂��߁A�J�����ʒu�����߂�.
	// �J�����͎����̈ʒu�A���Ă���_�i�����_�j�A�p���̂R�v�f����Ȃ�.
	D3DXVECTOR3 cam(0, 0, data->cam_transz);	// �J�����ʒu�� (0, 0, transz).
	D3DXVECTOR3 camAt(0, 0, 0); // �J�����̒����_�͌��_.
	D3DXVECTOR3 camUp(0, 1, 0); // �J�����͐����p�� up=(0, 1, 0).

	// �J�����ʒu��񂩂�s����쐬�BRH�͉E��n�̈Ӗ�.
	D3DXMATRIX camera;
	D3DXMatrixLookAtRH(&camera, &cam, &camAt, &camUp);

	// �J��������]������BSHIFT�L�[�{�}�E�X���{�^��.
	D3DXMATRIX mx, my;
	// X����Y�����S�̉�]���s��ŕ\��.
	D3DXMatrixRotationZ(&my, data->cam_roty);
	D3DXMatrixRotationX(&mx, data->cam_rotx);
	// camera�s��Ƃ����Z.
	camera = my * mx * camera;
	// �����Z�̏��Ԃ�ς���ƕ\�����ς��.
	//camera = mx * my * camera;

	// �r���[�s��Ƃ��Đݒ�.
	data->dev->SetTransform(D3DTS_VIEW, &camera);

	// ���e�s���ݒ肷��B�X�N���[���ɓ��e���邽�߂ɕK�v�B����p�̓�/3.
	D3DXMATRIX proj;
	D3DXMatrixPerspectiveFovRH(&proj, D3DX_PI/3, data->aspect, 0.1f, 200.0f);
	// �쐬�����s����A���e�s��ɐݒ�.
	data->dev->SetTransform(D3DTS_PROJECTION, &proj);
}

////////////////////////// ::setLight //////////////////////////
// ���C�g����ݒ肷��B�O���Ɩ���p����.
void setLight(DataSet *data) {
	// ���C�g�̌��ʂƈʒu�����߁A0�Ԃ̃��C�g�Ƃ��Đݒ�A�L���ɂ���.
	D3DLIGHT9 light;
	ZeroMemory(&light, sizeof(D3DLIGHT9));
	light.Type = D3DLIGHT_DIRECTIONAL;		// �f�B���N�V���i�����C�g.

	// �L�[���C�g�͋������F��.
	light.Diffuse.r = 1.0;
	light.Diffuse.g = 1.0;
	light.Diffuse.b = 1.0;
	light.Ambient.r = 0.2f;
	light.Ambient.g = 0.2f;
	light.Ambient.b = 0.2f;
	// ������(1,-1,-1)���������C�g.
	light.Direction.x = 1;
	light.Direction.y = -1;
	light.Direction.z = -1;
	data->dev->SetLight(0, &light);		// 0�Ԃɐݒ�.
	data->dev->LightEnable(0, data->light[0]);	// �L���ɂ���.

	// �t�B�����C�g�͎ア���F��.
	light.Diffuse.r = 0.6f;
	light.Diffuse.g = 0.6f;
	light.Diffuse.b = 0.6f;
	// ������(-1, 1,-1)���������C�g�B�O�ԃ��C�g�ƍ��E�ɋt����.
	light.Direction.x = -1;
	light.Direction.y = 1;
	light.Direction.z = -1;
	data->dev->SetLight(1, &light);		// 1�Ԃɐݒ�.
	data->dev->LightEnable(1, data->light[1]);	// �L���ɂ���.

	// �o�b�N���C�g�͋��߂�.
	light.Diffuse.r = 0.8f;
	light.Diffuse.g = 0.8f;
	light.Diffuse.b = 0.8f;
	// ������(-1, 1,1)���������C�g�B���f������납��Ƃ炷.
	light.Direction.x = -1;
	light.Direction.y = -1;
	light.Direction.z = 1;
	data->dev->SetLight(2, &light);		// 2�Ԃɐݒ�.
	data->dev->LightEnable(2, data->light[2]);	// �L���ɂ���.
}

////////////////////////// ::updateData //////////////////////////
// �f�[�^���A�b�v�f�[�g����.
// param data �f�[�^�Z�b�g.
void updateData(DataSet *data) {
	data->gameTimer++;		// �Q�[���^�C�}�[��i�s.

	// �������i�߂āA���݂��̏Փ˂��l��������.
	// divide���傫���قǌv�Z�P�ʂ��Z���Ȃ�.
	const int divide = 10;
	for(int ii = 0; ii < divide; ii++) {
		for(int bb = 0; data->ball[bb]; bb++) {

			data->ball[bb]->friction();
			data->ball[bb]->update(data, 1.0f/divide, ii);
		}
	}
}


////////////////////////// ::drawData //////////////////////////
// DataSet�Ɋ�Â��āA�X�v���C�g��`��.
// param data �f�[�^�Z�b�g.
// return ���������G���[.
HRESULT drawData(DataSet *data) {
	HRESULT hr = D3D_OK;

	// �w�i�F�����߂�BRGB=(200,200,255)�Ƃ���.
	D3DCOLOR rgb = D3DCOLOR_XRGB(200, 200, 250);
	// ��ʑS�̂�����.
	data->dev->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, rgb, 1.0f, 0);

	// �`����J�n�i�V�[���`��̊J�n�j.
	data->dev->BeginScene();

	// �J��������ݒ肷��.
	setCamera(data);
	// ���C�g����ݒ肷��.
	setLight(data);

	// �f�t�H���g�̓��C�e�B���OOFF.
	data->dev->SetRenderState(D3DRS_LIGHTING, data->light[3]);
	if (data->flag[0] == false) {
		data->dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	} else {
		data->dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	}

	// ���[���h���W�n�̐ݒ�͂܂��P�ʍs���.
	D3DXMATRIX ww;
	D3DXMatrixIdentity(&ww);
	data->dev->SetTransform(D3DTS_WORLD, &ww);

	// X�f�[�^�̕`��.
	data->raxa->draw(data);
	for(int bb = 0; data->ball[bb]; bb++) {
		data->ball[bb]->draw(data);
	}

	data->dev->EndScene();

	// ���ۂɉ�ʂɕ\���B�o�b�N�o�b�t�@����t�����g�o�b�t�@�ւ̓]��.
	// �f�o�C�X�������̃t���O�w��ɂ��A������VSYNC��҂�.
	data->dev->Present(NULL, NULL, NULL, NULL);

	float totalEnergy = 0;
	for(int bb = 0; data->ball[bb]; bb++) {
		totalEnergy += data->ball[bb]->energy();
	}

	// Direct3D�\����AGDI�ɂ�蕶����\��.
	HDC hdc = GetDC(data->hWnd);
	SetTextColor(hdc, RGB(0xFF, 0x00, 0x00));
	SetBkMode(hdc, TRANSPARENT);
	TCHAR text[256] = {0};
	_stprintf_s(text, _TEXT("LIGHT %s %d%d%d, cam rot=(%.2f, %.2f) z=%.2f flag %d%d%d, ee=%.5g"), 
		data->light[3]?_TEXT("ON"):_TEXT("OFF"), data->light[0], data->light[1], data->light[2], 
		data->cam_rotx, data->cam_roty, data->cam_transz, 
		data->flag[0], data->flag[1], data->flag[2], totalEnergy);
	TextOut(hdc, 20, 700, text, (int)_tcsclen(text));		// ������̕\��.
	ReleaseDC(data->hWnd, hdc);

	return D3D_OK;
}

// �C�x���g�����R�[���o�b�N�i�E�B���h�E�v���V�[�W���j.
// �C�x���g��������DispatchMessage�֐�����Ă΂��.
// param hWnd �C�x���g�̔��������E�B���h�E.
// param uMsg �C�x���g�̎�ނ�\��ID.
// param wParam ��ꃁ�b�Z�[�W�p�����[�^.
// param lParam ��񃁃b�Z�[�W�p�����[�^.
// return DefWindowProc�̖߂�l�ɏ]��.
//
LRESULT CALLBACK
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// �}�E�X�ʒu�L�^�p�ϐ�.
	static int mxx, myy;	
	// �C�x���g�̎�ނɉ����āAswitch���ɂď�����؂蕪����.
	switch(uMsg) {
	case WM_KEYDOWN:
		// ESC�L�[���������ꂽ��I��.
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
		// �{�^���������ꂽ��ʒu���L�^���Ă���.
		mxx = MAKEPOINTS(lParam).x;
		myy = MAKEPOINTS(lParam).y;
		break;

	case WM_MOUSEMOVE: {
		int xx = MAKEPOINTS(lParam).x;
		int yy = MAKEPOINTS(lParam).y;
		// ���{�^�������ŁA�J������]��.
		if (wParam & MK_LBUTTON) {
			if (wParam) {
				mydata.cam_roty -= (mxx - xx) * 0.01f;
				mydata.cam_rotx -= (myy - yy) * 0.01f;
			}
		}
		// �E�{�^�������ŁA�J�����̑O��ړ���.
		if (wParam & MK_RBUTTON) {
			mydata.cam_transz += (myy - yy) * 0.1f;
		}
		mxx = xx; myy = yy;
	}
		break;

	case WM_CLOSE:		// �I���ʒm(CLOSE�{�^���������ꂽ�ꍇ�Ȃ�)���͂����ꍇ.
		// �v���O�������I�������邽�߁A�C�x���g���[�v��0��ʒm����.
		// ���̌��ʁAGetMessage�̖߂�l��0�ɂȂ�.
		PostQuitMessage(0);
		break;
	default:
		break;
	}

	// �f�t�H���g�̃E�B���h�E�C�x���g����.
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Window���쐬����.
// return �E�B���h�E�n���h��.
//
HWND initWindow(DataSet *data) {
	// �܂��E�B���h�E�N���X��o�^����.
	// ����̓E�B���h�E������̏����̎d����Windows�ɋ����邽�߂ł���.
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));	// �ϐ�wc���[���N���A����.
	wc.cbSize = sizeof(WNDCLASSEX);			// ���̍\���̂̑傫����^����.
	wc.lpfnWndProc = (WNDPROC)WindowProc;	// �E�B���h�E�v���V�[�W���o�^.
	wc.hInstance = data->hInstance;				// �C���X�^���X�n���h����ݒ�.
	wc.hCursor = LoadCursor(NULL, IDC_CROSS);	// �}�E�X�J�[�\���̓o�^.
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);	// �w�i��GRAY��.
	wc.lpszClassName = wndClsName;			// �N���X���ACreateWindow�ƈ�v������.
	RegisterClassEx(&wc);			// �o�^.

	// �E�B���h�E���쐬����B�N���X����"directx".
	data->hWnd = CreateWindow(wndClsName, version, WS_OVERLAPPEDWINDOW, 
		0, 0, WIDTH, HEIGHT, NULL, NULL, data->hInstance, NULL);

	return data->hWnd;
}

// �e�N�X�`��fname��ǂݍ��ށB�G���[�������������O�𔭐�����.
// param fname �t�@�C�����Bconst�C���q�����֐����Œl��ύX���Ȃ����Ƃ�錾����.
// param tex �쐬����e�N�X�`���ւ̃|�C���^�����邽�߂̃|�C���^.
HRESULT loadTexture(IDirect3DDevice9 *dev, const TCHAR fname[], IDirect3DTexture9 **tex) {
	HRESULT hr = D3DXCreateTextureFromFile(dev, fname, tex);
	if ( FAILED(hr) ) {
		MessageBox(NULL, _TEXT("�e�N�X�`���ǂݍ��ݎ��s"), fname, MB_OK);
		throw hr;		// �G���[�����������̂ŗ�O�𑗂�.
	}
	return hr;
}

// ���C�����[�v.
// param hInstance �A�v���P�[�V������\���n���h��.
void MainLoop(DataSet *data) {
	HRESULT hr = E_FAIL;
	data->hWnd = initWindow(data);			// �E�B���h�E���쐬����.

	// Direct3D������������
	hr = initDirect3D(data);
	if ( FAILED(hr) ) {
		MessageBox(NULL, d3dErrStr(hr), _TEXT("Direct3D���������s"), MB_OK);
		return;
	}
	// �e��3D�`��ݒ���s��.
	data->dev->SetRenderState(D3DRS_ZENABLE, TRUE);		// Z�o�b�t�@�����O�L��.
	// ���C�gON.
	data->light[0] = data->light[1] = data->light[2] = TRUE;
	data->light[3] = TRUE;
	data->aspect = (float)WIDTH / (float)HEIGHT;
	data->cam_transz = 50;

	// raxa.x, ball.x��ǂݍ���.
	data->raxa = MyXData::load(data, _TEXT("data/raxa.x"));
  
	
	//�L���[�{�[��.
	data->ball[0] = new MyBall(data, _TEXT("data/ballB.x"), _TEXT("B"));
	data->ball[0]->move(-2.0f, 0.0f);//�����ʒu.
	data->ball[0]->vel(0.7f, 0.01f);//�������x.
	
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


	// �ՓˑΏۂ�o�^
	for (int i = 0; i < 10; i++) {

		if (data->ball[i] == nullptr)
		{
			continue; // i�Ԗڂ�null�Ȃ�X�L�b�v.
		}
		for (int j = 0; j < 10; j++) {
            //�����ԍ���null�Ȃ�X�L�b�v.
			if (i == j || 
				data->ball[j] == nullptr)
			{
				continue;
			}
			data->ball[i]->setTarget(data->ball[j]);
		}
	}

	data->flag[1] = false;		// �Đ�OFF.


	ShowWindow(data->hWnd, SW_SHOWNORMAL);	// �쐬�����E�B���h�E��\������.

	// �����v���̒��_�����w�ʂ��J�����O.
	//data->dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	// �J�����O�𖳌���.
	data->dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	// �C�x���g���[�v.
	// �u���b�N�^�֐�GetMessage�ł͂Ȃ��m���u���b�N�^�֐���PeekMessage���g��.
	MSG msg;
	bool flag = 1;
	while(flag) {
		// ���b�Z�[�W(����)�����邩�ǂ����m�F����.
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			// ���b�Z�[�W������̂ŏ�������.

			//msg�Ƀf�[�^���i�[.
			if (GetMessage(&msg, NULL, 0, 0) == 1) {
				TranslateMessage(&msg);//���̓C�x���g��K�؂ɏ����ł���悤�C��.
				DispatchMessage(&msg);//�E�B���h�E�v���V�[�W���ɑ��M.
			} else {
				flag = 0;
			}
		}
		else {//���͂ɂ�鏈���ƕ`�揈���͓����ɂ͍s���Ȃ�.
			if (data->flag[1] || data->flag[2]) {
				data->flag[2] = FALSE;
				updateData(data);	// �ʒu�̍Čv�Z.

				
			}
			drawData(data);		// �`��.
		}
	}
}

#define RELEASE(__xx__) if (__xx__) { __xx__->Release(); __xx__ = 0; }

// DataSet���������.
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

// param argc �R�}���h���C������n���ꂽ�����̐�.
// param argv �����̎��̂ւ̃|�C���^�z��.
// return 0 ����I��.
int main(int argc, char *argv[]) {
	srand((UINT)time(NULL));// �����n��̏�����.

	// ���̃v���O���������s�����Ƃ��̃C���X�^���X�n���h�����擾.
	mydata.hInstance = GetModuleHandle(NULL);
	MainLoop(&mydata);
	
	// �m�ۂ������\�[�X�����.
	ReleaseDataSet(&mydata);
	return 0;
}

// Direct3D������������.
// param data �f�[�^�Z�b�g.
// return ���������G���[�܂���D3D_OK.
//
HRESULT initDirect3D(DataSet *data) {
	HRESULT hr;

	// Direct3D�C���X�^���X�I�u�W�F�N�g�𐶐�����.
	// D3D_SDK_VERSION�ƁA�����^�C���o�[�W�����ԍ����K�؂łȂ��ƁANULL���Ԃ�.
	data->pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	// NULL�Ȃ烉���^�C�����s�K��.
	if (data->pD3D == NULL) return E_FAIL;

	// PRESENT�p�����[�^���[���N���A���A�K�؂ɏ�����.
	ZeroMemory(&(data->d3dpp), sizeof(data->d3dpp));
	// �E�B���h�E���[�h��.
	data->d3dpp.Windowed = TRUE;
	data->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	// �o�b�N�o�b�t�@��RGB���ꂼ��W�r�b�g��.
	data->d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	// Present���ɐ��������ɍ��킹��.
	data->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

	// 3D�p��Z�o�b�t�@(depth buffer)���쐬.
	data->d3dpp.EnableAutoDepthStencil = TRUE;
	data->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	// D3D�f�o�C�X�I�u�W�F�N�g�̍쐬.
	hr = data->pD3D ->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, data->hWnd, 
		D3DCREATE_HARDWARE_VERTEXPROCESSING, &(data->d3dpp), &(data->dev));
	if (FAILED(hr)) {
		// D3D�f�o�C�X�I�u�W�F�N�g�̍쐬�BHAL&SOFT.
		hr = data->pD3D ->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, data->hWnd, 
			D3DCREATE_SOFTWARE_VERTEXPROCESSING, &(data->d3dpp), &(data->dev));
		if (FAILED(hr)) {
			// D3D�f�o�C�X�I�u�W�F�N�g�̍쐬�BREF&SOFT.
			hr = data->pD3D ->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, data->hWnd, 
				D3DCREATE_SOFTWARE_VERTEXPROCESSING, &(data->d3dpp), &(data->dev));
		}
	}
	if ( FAILED(hr) ) return hr;

	return D3D_OK;
}

// �G���[��������HRESULT�𕶎���ɕϊ����邽�߂̕⏕�֐�.
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
