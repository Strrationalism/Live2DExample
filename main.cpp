// 内存泄露检测
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

const static inline auto ignore = _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

#include <string_view>
#include <algorithm>
#include <map>
#include "GraphicBuffer.h"
#include "Texture2D.h"
#include "RenderTarget.h"
#include "LAppAllocator.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/CubismFramework.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/Model/CubismModel.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/Model/CubismUserModel.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/CubismModelSettingJson.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/Rendering/D3D11/CubismRenderer_D3D11.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/Id/CubismIdManager.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/CubismDefaultParameterId.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/Physics/CubismPhysics.hpp"
#include "Cubism3SDKforNative-beta10/Framework/src/Motion/CubismMotion.hpp"
#include "Utils/CubismString.hpp"
#include "Motion/CubismMotionQueueEntry.hpp"

#include <Snowing.h>

using namespace Snowing;



int main()
{
	auto engine = PlatformImpls::WindowsImpl::MakeEngine(L"Live2D Demo", { 1024,768 }, true);

	// 用于Live2D的内存分配器
	LAppAllocator allocer;

	// Live2D启动选项
	Csm::CubismFramework::Option opt;
	opt.LogFunction = [] (const char* log){
		Log("Live2D", log);
	};
	opt.LoggingLevel = opt.LogLevel_Info;
	
	// 启动Live2D Framework
	Csm::CubismFramework::StartUp(&allocer, &opt);

	// 初始化
	Csm::CubismFramework::Initialize();

	// 加载模型设置信息
	const std::string dir = "Hiyori/";
	const char* fileName = "Hiyori.model3.json";
	auto settingJson = LoadAsset((dir + fileName).c_str());
	std::unique_ptr<Csm::ICubismModelSetting> setting = 
		std::make_unique<Csm::CubismModelSettingJson>(settingJson.Get<Csm::csmByte*>(), settingJson.Size());

	// 加载模型
	auto modelBlob = LoadAsset((dir + setting->GetModelFileName()).c_str());
	auto moc3Blob = LoadAsset((dir + "Hiyori.moc3").c_str());
	Csm::CubismMoc* moc = Csm::CubismMoc::Create(moc3Blob.Get<uint8_t*>(), moc3Blob.Size());
	auto model = moc->CreateModel();
	
	// 接入渲染环境
	Csm::Rendering::CubismRenderer_D3D11::InitializeConstantSettings(2, 
		PlatformImpls::WindowsImpl::D3D::Device::Get().GetHandler().Cast<IUnknown*,ID3D11Device*>());
	Csm::Rendering::CubismRenderer* renderer = Csm::Rendering::CubismRenderer::Create();
	renderer->Initialize(model);

	// 载入纹理
	auto tex0 = Graphics::LoadTexture(LoadAsset("Hiyori/Hiyori.2048/texture_00.ctx"));
	auto tex1 = Graphics::LoadTexture(LoadAsset("Hiyori/Hiyori.2048/texture_01.ctx"));
	dynamic_cast<Csm::Rendering::CubismRenderer_D3D11*>(renderer)->BindTexture(
		0, tex0.GetImpl().ShaderResource().Cast<IUnknown*, ID3D11ShaderResourceView*>());
	dynamic_cast<Csm::Rendering::CubismRenderer_D3D11*>(renderer)->BindTexture(
		1, tex1.GetImpl().ShaderResource().Cast<IUnknown*, ID3D11ShaderResourceView*>());

	// 指定坐标系
	Csm::CubismModelMatrix* modelMatrix = CSM_NEW Csm::CubismModelMatrix(model->GetCanvasWidth(), model->GetCanvasHeight());
	Csm::CubismMatrix44 projectionMatrix;
	projectionMatrix.Scale(1, 1024.0f / 768.0f);
	projectionMatrix.MultiplyByMatrix(modelMatrix);
	dynamic_cast<Csm::Rendering::CubismRenderer_D3D11*>(renderer)->SetMvpMatrix(&projectionMatrix);

	// 获取模型ID
	auto _idParamAngleX = Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamAngleX);
	auto _idParamAngleY = Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamAngleY);
	auto _idParamAngleZ = Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamAngleZ);
	auto _idParamBodyAngleX = Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamBodyAngleX);
	auto _idParamEyeBallX = Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamEyeBallX);
	auto _idParamEyeBallY = Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamEyeBallY);

	// 呼吸效果
	auto breath = Csm::CubismBreath::Create();
	Csm::csmVector<Csm::CubismBreath::BreathParameterData> breathParameters;
	breathParameters.PushBack(Csm::CubismBreath::BreathParameterData(_idParamAngleX, 0.0f, 15.0f, 6.5345f, 0.5f));
	breathParameters.PushBack(Csm::CubismBreath::BreathParameterData(_idParamAngleY, 0.0f, 8.0f, 3.5345f, 0.5f));
	breathParameters.PushBack(Csm::CubismBreath::BreathParameterData(_idParamAngleZ, 0.0f, 10.0f, 5.5345f, 0.5f));
	breathParameters.PushBack(Csm::CubismBreath::BreathParameterData(_idParamBodyAngleX, 0.0f, 4.0f, 15.5345f, 0.5f));
	breathParameters.PushBack(Csm::CubismBreath::BreathParameterData(
		Csm::CubismFramework::GetIdManager()->GetId(Csm::DefaultParameterId::ParamBreath), 0.5f, 0.5f, 3.2345f, 0.5f));
	breath->SetParameters(breathParameters);

	// 眨眼效果
	auto blink = Csm::CubismEyeBlink::Create();
	auto eyeBlinkIdCount = setting->GetEyeBlinkParameterCount();
	Csm::csmVector<Csm::CubismIdHandle> eyeParams;
	for (int i = 0; i < eyeBlinkIdCount; ++i)
	{
		eyeParams.PushBack(setting->GetEyeBlinkParameterId(i));
	}
	blink->SetParameterIds(eyeParams);
	blink->SetBlinkingInterval(1.0F);

	// 物理演算
	auto physicsBlob = LoadAsset((dir + setting->GetPhysicsFileName()).c_str());
	auto physics = Csm::CubismPhysics::Create(physicsBlob.Get<Csm::csmByte*>(), physicsBlob.Size());
	Csm::CubismPhysics::Options phyOpt;
	phyOpt.Wind.X = 1;
	physics->SetOptions(phyOpt);

	// 姿势
	auto poseBlob = LoadAsset((dir + setting->GetPoseFileName()).c_str());
	auto pose = Csm::CubismPose::Create(poseBlob.Get<Csm::csmByte*>(), poseBlob.Size());

	// 表情
	std::map<std::string, Csm::CubismExpressionMotion*> expressions;
	const size_t expressionCount = setting->GetExpressionCount();
	for (size_t i = 0; i < expressionCount; i++)
	{
		auto name = setting->GetExpressionName(i);
		auto path = setting->GetExpressionFileName(i);
		auto blob = LoadAsset((dir + path).c_str());

		auto motion = Csm::CubismExpressionMotion::Create(blob.Get<Csm::csmByte*>(), blob.Size());

		expressions[name] = motion;
	}

	// Layout
	Csm::csmMap<Csm::csmString, Csm::csmFloat32> layout;
	setting->GetLayoutMap(layout);
	modelMatrix->SetupFromLayout(layout);
	model->SaveParameters();

	// 嘴型同步参数ID
	Csm::csmVector<Csm::CubismIdHandle> lipParams;

	// 动作的某个组件，用处未知
	auto motionEntry = Csm::CubismMotionQueueEntry{};

	// 动作管理器
	Csm::CubismMotionManager motionManager;

	// 动作
	Csm::CubismMotion* someMotion = nullptr;

	std::map<std::string, std::map<std::string, Csm::CubismMotion*>> motionGroups;
	for (size_t i = 0; i < setting->GetMotionGroupCount(); i++)
	{
		auto groupName = setting->GetMotionGroupName(i);
		std::map<std::string, Csm::CubismMotion*>& group = motionGroups[groupName];

		const size_t count = setting->GetMotionCount(groupName);

		for (size_t i = 0; i < count; i++)
		{
			//ex) idle_0
			auto name = Csm::Utils::CubismString::GetFormatedString("%s_%d", group, i);
			auto path = setting->GetMotionFileName(groupName, i);
			auto blob = LoadAsset((dir + path).c_str());

			Csm::CubismMotion* tmpMotion = Csm::CubismMotion::Create(blob.Get<Csm::csmByte*>(), blob.Size());

			float fadeTime = setting->GetMotionFadeInTimeValue(groupName, i);
			if (fadeTime >= 0.0f)
				tmpMotion->SetFadeInTime(fadeTime);
			

			fadeTime = setting->GetMotionFadeOutTimeValue(groupName, i);
			if (fadeTime >= 0.0f)
				tmpMotion->SetFadeOutTime(fadeTime);
			
			tmpMotion->SetEffectIds(eyeParams, lipParams);

			group[name.GetRawString()] = tmpMotion;

			tmpMotion->IsLoop(true);

			//if(!someMotion) 
				someMotion = tmpMotion;

		}
	}
	

	// 执行动作
	motionManager.GetCubismMotionQueueEntry(0);
	motionManager.StartMotion(someMotion, false, 100);
	
	// 准备ID3D11Device*
	auto device = PlatformImpls::WindowsImpl::D3D::Device::Get().GetHandler().Cast<IUnknown*, ID3D11Device*>();

	Engine::Get().Run([&] {
		
		// 基本渲染环境
		Graphics::Device::MainContext().ClearRenderTarget(
			Graphics::Device::MainRenderTarget());

		Graphics::Device::MainContext().SetRenderTarget(
			&Graphics::Device::MainRenderTarget());

		// 更新呼吸效果
		breath->UpdateParameters(model, Engine::Get().DeltaTime());

		// 更新Pose
		pose->UpdateParameters(model, Engine::Get().DeltaTime());

		// 更新动作
		motionManager.UpdateMotion(model, Engine::Get().DeltaTime());

		// 更新眨眼效果
		blink->UpdateParameters(model, Engine::Get().DeltaTime());

		// 物理演算
		physics->Evaluate(model, Engine::Get().DeltaTime());

		// 更新模型
		model->Update();

		// 开始一帧绘制
		dynamic_cast<Csm::Rendering::CubismRenderer_D3D11*>(renderer)->StartFrame(
			device,
			Graphics::Device::MainContext().GetImpl().GetHandler().Cast<IUnknown*, ID3D11DeviceContext*>(),
			1024, 768);

		// 绘制模型
		dynamic_cast<Csm::Rendering::CubismRenderer_D3D11*>(renderer)->DrawModel();

		// 结束绘制
		dynamic_cast<Csm::Rendering::CubismRenderer_D3D11*>(renderer)->EndFrame(device);
	});

	// 终期化
	for (auto& p : expressions)
		Csm::CubismExpressionMotion::Delete(p.second);

	Csm::CubismEyeBlink::Delete(blink);
	Csm::CubismPose::Delete(pose);
	Csm::CubismPhysics::Delete(physics);
	Csm::CubismBreath::Delete(breath);
	CSM_DELETE(modelMatrix);
	Csm::Rendering::CubismRenderer::Delete(renderer);
	moc->DeleteModel(model);
	Csm::CubismMoc::Delete(moc);
	Csm::CubismFramework::Dispose();

	return 0;
}
