#include "LLGI.Graphics.h"
#include "LLGI.ConstantBuffer.h"
#include "LLGI.Texture.h"

namespace LLGI
{

//! TODO should be moved
static std::function<void(LogType, const char*)> g_logger;

void SetLogger(const std::function<void(LogType, const char*)>& logger) { g_logger = logger; }

void Log(LogType logType, const char* message)
{
	if (g_logger != nullptr)
	{
		g_logger(logType, message);
	}
}

SingleFrameMemoryPool::SingleFrameMemoryPool(int32_t swapBufferCount) : swapBufferCount_(swapBufferCount)
{

	for (int i = 0; i < swapBufferCount_; i++)
	{
		offsets_.push_back(0);
		constantBuffers_.push_back(std::vector<ConstantBuffer*>());
	}
}

SingleFrameMemoryPool::~SingleFrameMemoryPool()
{
	for (auto& constantBuffer : constantBuffers_)
	{
		for (auto c : constantBuffer)
		{
			c->Release();
		}
	}
}

void SingleFrameMemoryPool::NewFrame()
{
	currentSwapBuffer_++;
	currentSwapBuffer_ %= swapBufferCount_;
	offsets_[currentSwapBuffer_] = 0;
}

ConstantBuffer* SingleFrameMemoryPool::CreateConstantBuffer(int32_t size)
{
	assert(currentSwapBuffer_ >= 0);

	if (constantBuffers_[currentSwapBuffer_].size() <= offsets_[currentSwapBuffer_])
	{
		auto cb = CreateConstantBufferInternal(size);
		if (cb == nullptr)
		{
			return nullptr;
		}

		constantBuffers_[currentSwapBuffer_].push_back(cb);
		SafeAddRef(cb);
		offsets_[currentSwapBuffer_]++;
		return cb;
	}
	else
	{
		auto cb = constantBuffers_[currentSwapBuffer_][offsets_[currentSwapBuffer_]];
		auto newCb = ReinitializeConstantBuffer(cb, size);
		if (newCb == nullptr)
		{
			return nullptr;
		}

		SafeAddRef(newCb);
		offsets_[currentSwapBuffer_]++;
		return newCb;
	}

	return nullptr;
}

bool RenderPass::assignRenderTextures(Texture** textures, int32_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (!(textures[i]->GetType() == TextureType::Render || textures[i]->GetType() == TextureType::Screen))
		{
			Log(LogType::Error, "RenderPass : Invalid RenderTexture.");
			return false;
		}
	}

	for (size_t i = 0; i < count; i++)
	{
		SafeAddRef(textures[i]);
	}

	for (size_t i = 0; i < renderTextures_.size(); i++)
	{
		renderTextures_.at(i)->Release();
	}

	renderTextures_.resize(count);

	for (size_t i = 0; i < count; i++)
	{
		renderTextures_.at(i) = textures[i];
	}

	return true;
}

bool RenderPass::assignDepthTexture(Texture* depthTexture)
{
	if (depthTexture != nullptr && depthTexture->GetType() != TextureType::Depth)
	{
		Log(LogType::Error, "RenderPass : Invalid DepthTexture.");
		return false;
	}

	SafeAddRef(depthTexture);
	SafeRelease(depthTexture_);
	depthTexture_ = depthTexture;

	return true;
}

bool RenderPass::getSize(Vec2I& size, const Texture** textures, int32_t textureCount, Texture* depthTexture) const
{
	if (textureCount == 0)
	{
		Log(LogType::Error, "RenderPass : Invalid Count.");
		return false;
	}

	size = textures[0]->GetSizeAs2D();

	for (int i = 0; i < textureCount; i++)
	{
		auto temp = textures[i]->GetSizeAs2D();
		if (size.X != temp.X)
			goto FAIL;
		if (size.Y != temp.Y)
			goto FAIL;
	}

	if (depthTexture != nullptr)
	{
		if (size.X != depthTexture->GetSizeAs2D().X)
			goto FAIL;
		if (size.Y != depthTexture->GetSizeAs2D().Y)
			goto FAIL;
	}

	return true;

FAIL:;
	Log(LogType::Error, "RenderPass : Invalid Size.");
	return false;
}

RenderPass::~RenderPass()
{
	SafeRelease(depthTexture_);

	for (size_t i = 0; i < renderTextures_.size(); i++)
	{
		renderTextures_.at(i)->Release();
	}
}

void RenderPass::SetIsColorCleared(bool isColorCleared) { isColorCleared_ = isColorCleared; }

void RenderPass::SetIsDepthCleared(bool isDepthCleared) { isDepthCleared_ = isDepthCleared; }

void RenderPass::SetClearColor(const Color8& color) { color_ = color; }

bool RenderPass::GetIsSwapchainScreen() const { return GetRenderTexture(0)->GetType() == TextureType::Screen; }

RenderPassPipelineStateKey RenderPass::GetKey() const
{
	RenderPassPipelineStateKey key;

	key.IsPresent = GetIsSwapchainScreen();
	key.IsColorCleared = GetIsColorCleared();
	key.IsDepthCleared = GetIsDepthCleared();
	key.RenderTargetFormats.resize(GetRenderTextureCount());

	for (size_t i = 0; i < key.RenderTargetFormats.size(); i++)
	{
		key.RenderTargetFormats.at(i) = GetRenderTexture(static_cast<int32_t>(i))->GetFormat();
	}

	if (GetHasDepthTexture())
	{
		key.DepthFormat = GetDepthTexture()->GetFormat();
	}
	else
	{
		key.DepthFormat = TextureFormatType::Unknown;
	}

	if (key.RenderTargetFormats.at(0) == TextureFormatType::Unknown)
	{
		printf("");
	}

	return key;
}

Graphics::~Graphics()
{
	if (disposed_ != nullptr)
	{
		disposed_();
	}
}

void Graphics::SetWindowSize(const Vec2I& windowSize) { windowSize_ = windowSize; }

void Graphics::Execute(CommandList* commandList) {}

// RenderPass* Graphics::GetCurrentScreen(const Color8& clearColor, bool isColorCleared, bool isDepthCleared) { return nullptr; }

VertexBuffer* Graphics::CreateVertexBuffer(int32_t size) { return nullptr; }

IndexBuffer* Graphics::CreateIndexBuffer(int32_t stride, int32_t count) { return nullptr; }

Shader* Graphics::CreateShader(DataStructure* data, int32_t count) { return nullptr; }

PipelineState* Graphics::CreatePiplineState() { return nullptr; }

SingleFrameMemoryPool* Graphics::CreateSingleFrameMemoryPool(int32_t constantBufferPoolSize, int32_t drawingCount) { return nullptr; }

CommandList* Graphics::CreateCommandList(SingleFrameMemoryPool* memoryPool) { return nullptr; }

ConstantBuffer* Graphics::CreateConstantBuffer(int32_t size) { return nullptr; }

Texture* Graphics::CreateTexture(uint64_t id) { return nullptr; }

RenderPassPipelineState* Graphics::CreateRenderPassPipelineState(RenderPass* renderPass) { return nullptr; }

std::vector<uint8_t> Graphics::CaptureRenderTarget(Texture* renderTarget)
{
	Log(LogType::Error, "GetColorBuffer is not implemented.");
	assert(0);
	return std::vector<uint8_t>();
}

void Graphics::SetDisposed(const std::function<void()>& disposed) { disposed_ = disposed; }

} // namespace LLGI
