#include "HelloTriangleShowcase.h"
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/demo/ShowcaseRegistry.h>
#include <showcase/core/Log.h>
#include <imgui.h>
#include <DirectXMath.h>
#include <cmath>

namespace showcase {

REGISTER_SHOWCASE(HelloTriangleShowcase)

struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

void HelloTriangleShowcase::OnLoad(RenderContext& ctx) {
    auto* device = ctx.GetDevice().GetDevice();

    // Create empty root signature (no resources needed)
    m_rootSignature = RootSignatureBuilder()
        .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
        .Build(device);

    // Load shaders
    auto vs = ctx.GetShaderManager().LoadShader("shaders/triangle_vs_vs.cso");
    auto ps = ctx.GetShaderManager().LoadShader("shaders/triangle_ps_ps.cso");

    // Create PSO
    GraphicsPipelineDesc psoDesc;
    psoDesc.rootSignature = m_rootSignature.Get();
    psoDesc.vertexShader = vs;
    psoDesc.pixelShader = ps;
    psoDesc.inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    psoDesc.rtvFormat = ctx.GetSwapChain().GetFormat();
    psoDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
    psoDesc.depthStencilState.DepthEnable = FALSE;
    psoDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    m_pipelineState = PipelineState::CreateGraphicsPSO(device, psoDesc);

    // Create vertex buffer
    Vertex vertices[] = {
        {{0.0f,  0.5f, 0.0f}, {1.0f, 0.2f, 0.2f, 1.0f}},  // Top - Red
        {{0.43f, -0.25f, 0.0f}, {0.2f, 1.0f, 0.2f, 1.0f}}, // Bottom Right - Green
        {{-0.43f, -0.25f, 0.0f}, {0.2f, 0.2f, 1.0f, 1.0f}}, // Bottom Left - Blue
    };

    m_vertexBuffer.InitAsVertexBuffer(device, ctx.GetDevice().GetAllocator(),
                                       vertices, sizeof(vertices), sizeof(Vertex));

    SE_LOG_INFO("HelloTriangle showcase loaded");
}

void HelloTriangleShowcase::OnUnload() {
    m_vertexBuffer.Shutdown();
    m_pipelineState.Reset();
    m_rootSignature.Reset();
}

void HelloTriangleShowcase::OnResize(uint32_t width, uint32_t height) {
    // Nothing to do for this simple demo
}

void HelloTriangleShowcase::OnUpdate(float deltaTime) {
    m_currentAngle += m_rotationSpeed * deltaTime;

    if (m_rotationSpeed != 0.0f) {
        // Update vertex positions with rotation
        float cos_a = std::cos(m_currentAngle);
        float sin_a = std::sin(m_currentAngle);

        // Original positions
        DirectX::XMFLOAT3 origPositions[] = {
            {0.0f,  0.5f, 0.0f},
            {0.43f, -0.25f, 0.0f},
            {-0.43f, -0.25f, 0.0f},
        };

        Vertex vertices[3];
        DirectX::XMFLOAT4 colors[] = {
            {1.0f, 0.2f, 0.2f, 1.0f},
            {0.2f, 1.0f, 0.2f, 1.0f},
            {0.2f, 0.2f, 1.0f, 1.0f},
        };

        for (int i = 0; i < 3; i++) {
            vertices[i].position.x = origPositions[i].x * cos_a - origPositions[i].y * sin_a;
            vertices[i].position.y = origPositions[i].x * sin_a + origPositions[i].y * cos_a;
            vertices[i].position.z = 0.0f;
            vertices[i].color = colors[i];
        }

        m_vertexBuffer.UpdateData(vertices, sizeof(vertices));
    }
}

void HelloTriangleShowcase::OnRender(RenderContext& ctx) {
    auto* cmdList = ctx.GetCommandList().Get();

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_pipelineState.Get());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    auto vbView = m_vertexBuffer.GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);

    cmdList->DrawInstanced(3, 1, 0, 0);
}

void HelloTriangleShowcase::OnImGui() {
    ImGui::SliderFloat("Rotation Speed", &m_rotationSpeed, 0.0f, 5.0f);

    if (ImGui::Button("Reset Rotation")) {
        m_currentAngle = 0.0f;
        m_rotationSpeed = 0.0f;
    }

    ImGui::Text("Current Angle: %.1f deg", m_currentAngle * 180.0f / 3.14159f);
}

} // namespace showcase
