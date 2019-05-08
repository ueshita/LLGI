
#include "LLGI.CommandListDX12.h"
#include "LLGI.GraphicsDX12.h"
#include "LLGI.IndexBufferDX12.h"
#include "LLGI.PipelineStateDX12.h"
#include "LLGI.VertexBufferDX12.h"

namespace LLGI
{

CommandListDX12::CommandListDX12() {}

CommandListDX12::~CommandListDX12() {}

bool CommandListDX12::Initialize(GraphicsDX12* graphics)
{
	SafeAddRef(graphics);
	graphics_ = CreateSharedPtr(graphics);

	for (int32_t i = 0; i < graphics_->GetSwapBufferCount(); i++)
	{
		ID3D12CommandAllocator* commandAllocator = nullptr;
		ID3D12GraphicsCommandList* commandList = nullptr;
		HRESULT hr;

		hr = graphics_->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
		if (FAILED(hr))
		{
			goto FAILED_EXIT;
		}
		commandAllocators.push_back(CreateSharedPtr(commandAllocator));

		hr = graphics_->GetDevice()->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, NULL, IID_PPV_ARGS(&commandList));
		if (FAILED(hr))
		{
			goto FAILED_EXIT;
		}
		commandList->Close();
		commandLists.push_back(CreateSharedPtr(commandList));
	}

	return true;

FAILED_EXIT:;
	graphics_.reset();
	commandAllocators.clear();
	commandLists.clear();
	return false;
}

void CommandListDX12::Begin()
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];
	commandList->Reset(commandAllocators[graphics_->GetCurrentSwapBufferIndex()].get(), nullptr);
	CommandList::Begin();
}

void CommandListDX12::End()
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];

	commandList->Close();
}

void CommandListDX12::BeginRenderPass(RenderPass* renderPass)
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];

	SafeAddRef(renderPass);
	renderPass_ = CreateSharedPtr((RenderPassDX12*)renderPass);

	if (renderPass != nullptr)
	{
		// Reset scissor
		D3D12_RECT rect;
		rect.top = 0;
		rect.left = 0;
		rect.right = renderPass_->screenWindowSize.X;
		rect.bottom = renderPass_->screenWindowSize.Y;
		commandList->RSSetScissorRects(1, &rect);

		// Clear color
		if (renderPass_->GetIsColorCleared())
		{
			Clear(renderPass_->GetClearColor());
		}
	}
}

void CommandListDX12::EndRenderPass() { renderPass_.reset(); }

void CommandListDX12::Draw(int32_t pritimiveCount)
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];

	BindingVertexBuffer vb_;
	IndexBuffer* ib_ = nullptr;
	PipelineState* pip_ = nullptr;

	bool isVBDirtied = false;
	bool isIBDirtied = false;
	bool isPipDirtied = false;

	GetCurrentVertexBuffer(vb_, isVBDirtied);
	GetCurrentIndexBuffer(ib_, isIBDirtied);
	GetCurrentPipelineState(pip_, isPipDirtied);

	assert(vb_.vertexBuffer != nullptr);
	assert(ib_ != nullptr);
	assert(pip_ != nullptr);

	auto vb = static_cast<VertexBufferDX12*>(vb_.vertexBuffer);
	auto ib = static_cast<IndexBufferDX12*>(ib_);
	auto pip = static_cast<PipelineStateDX12*>(pip_);

	if (vb_.vertexBuffer != nullptr)
	{
		D3D12_VERTEX_BUFFER_VIEW vertexView;
		vertexView.BufferLocation = vb->Get()->GetGPUVirtualAddress();
		vertexView.StrideInBytes = vb_.stride;
		vertexView.SizeInBytes = vb_.stride;	// is it true?
		commandList->IASetVertexBuffers(0, 1, &vertexView);
	}

	if (ib != nullptr)
	{
		D3D12_INDEX_BUFFER_VIEW indexView;
		indexView.BufferLocation = ib->Get()->GetGPUVirtualAddress();
		indexView.SizeInBytes = sizeof(uint16_t) * ib->GetCount();
		commandList->IASetIndexBuffer(&indexView);
	}

	if (pip != nullptr)
	{
		commandList->SetGraphicsRootSignature(pip->GetRootSignature());
		auto p = pip->GetPipelineState();
		commandList->SetPipelineState(p);
	}

	CommandList::Draw(pritimiveCount);
}

void CommandListDX12::Clear(const Color8& color)
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];

	auto rt = renderPass_;
	if (rt == nullptr)
		return;

	float color_[] = {color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f};

	commandList->ClearRenderTargetView(rt->handleRTV, color_, 0, nullptr);
}

ID3D12GraphicsCommandList* CommandListDX12::GetCommandList() const
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];
	return commandList.get();
}

} // namespace LLGI
