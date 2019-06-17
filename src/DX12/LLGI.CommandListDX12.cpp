
#include "LLGI.CommandListDX12.h"
#include "LLGI.ConstantBufferDX12.h"
#include "LLGI.GraphicsDX12.h"
#include "LLGI.IndexBufferDX12.h"
#include "LLGI.PipelineStateDX12.h"
#include "LLGI.TextureDX12.h"
#include "LLGI.VertexBufferDX12.h"

namespace LLGI
{

DescriptorHeapDX12::DescriptorHeapDX12(std::shared_ptr<GraphicsDX12> graphics, int size, int stage)
	: graphics_(graphics), size_(size), stage_(stage)
{
	for (int i = 0; i <= static_cast<int>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER); i++)
	{
		auto heap = CreateHeap(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i), size_ * stage_);
		assert(heap != nullptr);
		descriptorHeaps_[i] = heap;
		CpuHandles_[i] = descriptorHeaps_[i]->GetCPUDescriptorHandleForHeapStart();
		GpuHandles_[i] = descriptorHeaps_[i]->GetGPUDescriptorHandleForHeapStart();
	}
}

DescriptorHeapDX12::~DescriptorHeapDX12()
{
	SafeRelease(graphics_);
	for (int i = 0; i <= static_cast<int>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER); i++)
		SafeRelease(descriptorHeaps_[i]);

	// for (auto c : cache_)
	//{
	//	c.clear();
	//}
	// cache_.clear();
}

void DescriptorHeapDX12::Increment(D3D12_DESCRIPTOR_HEAP_TYPE heapType, int count)
{
	if (heapType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && heapType != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		throw "not implemented";

	auto size = graphics_->GetDevice()->GetDescriptorHandleIncrementSize(heapType);
	auto i = static_cast<int>(heapType);
	CpuHandles_[i].ptr += size * count;
	GpuHandles_[i].ptr += size * count;
}
ID3D12DescriptorHeap* DescriptorHeapDX12::GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	if (heapType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && heapType != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		throw "not implemented";

	return descriptorHeaps_[static_cast<int>(heapType)];
}
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapDX12::GetCpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	if (heapType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && heapType != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		throw "not implemented";

	return CpuHandles_[static_cast<int>(heapType)];
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapDX12::GetGpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	if (heapType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && heapType != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		throw "not implemented";

	return GpuHandles_[static_cast<int>(heapType)];
}

void DescriptorHeapDX12::Reset()
{
	for (int i = 0; i <= static_cast<int>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER); i++)
	{
		CpuHandles_[i] = descriptorHeaps_[i]->GetCPUDescriptorHandleForHeapStart();
		GpuHandles_[i] = descriptorHeaps_[i]->GetGPUDescriptorHandleForHeapStart();
	}
}

ID3D12DescriptorHeap* DescriptorHeapDX12::CreateHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, int numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	ID3D12DescriptorHeap* heap = nullptr;

	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = heapType;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// TODO: set properly for multi-adaptor.
	heapDesc.NodeMask = 1;

	auto hr = graphics_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
	if (FAILED(hr))
	{
		SafeRelease(heap);
		return nullptr;
	}
	return heap;
}

CommandListDX12::CommandListDX12() {}

CommandListDX12::~CommandListDX12() { descriptorHeaps_.clear(); }

bool CommandListDX12::Initialize(GraphicsDX12* graphics)
{
	HRESULT hr;

	SafeAddRef(graphics);
	graphics_ = CreateSharedPtr(graphics);

	for (int32_t i = 0; i < graphics_->GetSwapBufferCount(); i++)
	{
		ID3D12CommandAllocator* commandAllocator = nullptr;
		ID3D12GraphicsCommandList* commandList = nullptr;

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

	for (size_t i = 0; i < static_cast<size_t>(graphics_->GetSwapBufferCount()); i++)
	{
		auto dp = std::make_shared<DescriptorHeapDX12>(graphics_, 100, 2);
		descriptorHeaps_.push_back(dp);
	}

	return true;

FAILED_EXIT:;
	graphics_.reset();
	commandAllocators.clear();
	commandLists.clear();
	descriptorHeaps_.clear();

	return false;
}

void CommandListDX12::Begin()
{
	auto commandList = commandLists[graphics_->GetCurrentSwapBufferIndex()];
	commandList->Reset(commandAllocators[graphics_->GetCurrentSwapBufferIndex()].get(), nullptr);

	auto& dp = descriptorHeaps_[graphics_->GetCurrentSwapBufferIndex()];
	dp->Reset();

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
		// Set render target
		commandList->OMSetRenderTargets(1, &(renderPass_->handleRTV), FALSE, nullptr);

		// TODO depth...

		// Reset scissor
		D3D12_RECT rect;
		rect.top = 0;
		rect.left = 0;
		rect.right = renderPass_->screenWindowSize.X;
		rect.bottom = renderPass_->screenWindowSize.Y;
		commandList->RSSetScissorRects(1, &rect);

		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = renderPass_->screenWindowSize.X;
		viewport.Height = renderPass_->screenWindowSize.Y;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);

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
	ConstantBuffer* cb = nullptr;
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

	{
		D3D12_VERTEX_BUFFER_VIEW vertexView;
		vertexView.BufferLocation = vb->Get()->GetGPUVirtualAddress() + vb_.offset;
		vertexView.StrideInBytes = vb_.stride;
		vertexView.SizeInBytes = vb_.vertexBuffer->GetSize() - vb_.offset;
		if (vb_.vertexBuffer != nullptr)
		{
			commandList->IASetVertexBuffers(0, 1, &vertexView);
		}
	}

	if (ib != nullptr)
	{
		D3D12_INDEX_BUFFER_VIEW indexView;
		indexView.BufferLocation = ib->Get()->GetGPUVirtualAddress();
		indexView.SizeInBytes = ib->GetStride() * ib->GetCount();
		indexView.Format = ib->GetStride() == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		commandList->IASetIndexBuffer(&indexView);
	}

	if (pip != nullptr)
	{
		commandList->SetGraphicsRootSignature(pip->GetRootSignature());
		auto p = pip->GetPipelineState();
		commandList->SetPipelineState(p);
	}

	auto& descriptorHeaps = descriptorHeaps_[graphics_->GetCurrentSwapBufferIndex()];

	// constant buffer
	{
		auto heap = descriptorHeaps->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		auto cpuHandle = descriptorHeaps->GetCpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		auto gpuHandle = descriptorHeaps->GetGpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHeaps->Increment(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);

		auto incrementSize = graphics_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		commandList->SetDescriptorHeaps(1, &heap);

		for (int stage_ind = 0; stage_ind < (int)ShaderStageType::Max; stage_ind++)
		{
			GetCurrentConstantBuffer(static_cast<ShaderStageType>(stage_ind), cb);
			if (cb != nullptr)
			{
				auto _cb = static_cast<ConstantBufferDX12*>(cb);
				D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
				desc.BufferLocation = _cb->Get()->GetGPUVirtualAddress();
				desc.SizeInBytes = _cb->GetSize();

				graphics_->GetDevice()->CreateConstantBufferView(&desc, cpuHandle);
			}
			cpuHandle.ptr += incrementSize;
		}
		commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);
	}

	{
		auto heapSrv = descriptorHeaps->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		auto cpuHandleSrv = descriptorHeaps->GetCpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		auto gpuHandleSrv = descriptorHeaps->GetGpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHeaps->Increment(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
								   1); // TODO: (int32_t)ShaderStageType::Max * currentTextures[stage_ind].size() ?

		// auto incrementSizeSrv = graphics_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		commandList->SetDescriptorHeaps(1, &heapSrv);

		auto heapSmp = descriptorHeaps->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		auto cpuHandleSmp = descriptorHeaps->GetCpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		auto gpuHandleSmp = descriptorHeaps->GetGpuHandle(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		descriptorHeaps->Increment(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
								   1); // TODO: (int32_t)ShaderStageType::Max * currentTextures[stage_ind].size() ?

		// auto incrementSizeSmp = graphics_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		commandList->SetDescriptorHeaps(1, &heapSmp);

		for (int stage_ind = 0; stage_ind < (int32_t)ShaderStageType::Max; stage_ind++)
		{
			for (int unit_ind = 0; unit_ind < currentTextures[stage_ind].size(); unit_ind++)
			{
				if (currentTextures[stage_ind][unit_ind].texture == nullptr)
					continue;

				auto texture = static_cast<TextureDX12*>(currentTextures[stage_ind][unit_ind].texture);
				auto wrapMode = currentTextures[stage_ind][unit_ind].wrapMode;
				auto minMagFilter = currentTextures[stage_ind][unit_ind].minMagFilter;

				// SRV
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srvDesc.Texture2D.MipLevels = 1;
					srvDesc.Texture2D.MostDetailedMip = 0;

					graphics_->GetDevice()->CreateShaderResourceView(texture->Get(), &srvDesc, cpuHandleSrv);
					commandList->SetGraphicsRootDescriptorTable(1, gpuHandleSrv);
				}

				// Sampler
				{
					D3D12_SAMPLER_DESC samplerDesc = {};

					// TODO
					samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

					if (wrapMode == TextureWrapMode::Repeat)
					{
						samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
						samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
						samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					}
					else
					{
						samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
						samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
						samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
					}
					samplerDesc.MipLODBias = 0;
					samplerDesc.MaxAnisotropy = 0;
					samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
					samplerDesc.MinLOD = 0.0f;
					samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

					graphics_->GetDevice()->CreateSampler(&samplerDesc, cpuHandleSmp);
					commandList->SetGraphicsRootDescriptorTable(2, gpuHandleSmp);
				}
			}
		}
	}

	// setup a topology (triangle)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// draw polygon
	commandList->DrawIndexedInstanced(pritimiveCount * 3 /*triangle*/, 1, 0, 0, 0);

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
