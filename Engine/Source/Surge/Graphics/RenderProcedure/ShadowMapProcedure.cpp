// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RenderProcedure/ShadowMapProcedure.hpp"
#include "Surge/ECS/Scene.hpp"
#include "GeometryProcedure.hpp"
#define NUM_FRUSTUM_CORNERS 8

namespace Surge
{
    struct ShadowParams
    {
        glm::vec4 CascadeEnds;
        glm::mat4 LightSpaceMatrix[4];
        int ShowCascades;
        glm::vec3 _Padding_;
    };

    void ShadowMapProcedure::Init(RendererData* rendererData)
    {
        mRendererData = rendererData;

        //TODO: Choose depending on hardware
        mProcData.CascadeCount = 4;
        mProcData.ShadowMapResolution = 4096;
        mProcData.LightViewProjections.resize(mProcData.CascadeCount);
        mCascadeSplits.resize(mProcData.CascadeCount);
        mProcData.CascadeSplitDepths.resize(mProcData.CascadeCount);

        Ref<Shader> mainPBRshader = mRendererData->ShaderSet.GetShader("PBR");
        Ref<Shader> shadowMapShader = mRendererData->ShaderSet.GetShader("ShadowMap");

        // Framebuffers
        FramebufferSpecification spec = {};
        spec.Formats = {ImageFormat::Depth32};
        spec.Width = mProcData.ShadowMapResolution;
        spec.Height = mProcData.ShadowMapResolution;
        mProcData.ShadowMapFramebuffers.resize(mProcData.CascadeCount);
        for (Uint i = 0; i < mProcData.ShadowMapFramebuffers.size(); i++)
            mProcData.ShadowMapFramebuffers[i] = Framebuffer::Create(spec);

        // Pipelines
        GraphicsPipelineSpecification pipelineSpec {};
        pipelineSpec.Shader = shadowMapShader;
        pipelineSpec.Topology = PrimitiveTopology::TriangleList;
        pipelineSpec.CullingMode = CullMode::Back;
        pipelineSpec.UseDepth = true;
        pipelineSpec.UseStencil = false;
        pipelineSpec.DebugName = "ShadowMapPipeline";
        pipelineSpec.LineWidth = 1.0f;
        mProcData.ShadowMapPipelines.resize(mProcData.CascadeCount);
        for (Uint i = 0; i < mProcData.ShadowMapPipelines.size(); i++)
        {
            pipelineSpec.TargetFramebuffer = mProcData.ShadowMapFramebuffers[i];
            mProcData.ShadowMapPipelines[i] = GraphicsPipeline::Create(pipelineSpec);
        }

        mProcData.ShadowDesciptorSet = DescriptorSet::Create(mainPBRshader, 3, false);
        mProcData.ShadowUniformBuffer = UniformBuffer::Create(sizeof(ShadowParams));
    }

    void ShadowMapProcedure::CalculateMatricesAndUpdateCBuffer(const glm::mat4& viewProjection, const glm::vec3& normalizedDirection)
    {
        glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

        // TODO: Automate this
        const float nearClip = 0.1f;
        const float farClip = 1000.0f;

        const float clipRange = farClip - nearClip;
        // Calculate the optimal cascade distances
        const float minZ = nearClip;
        const float maxZ = nearClip + clipRange;
        const float range = maxZ - minZ;
        const float ratio = maxZ / minZ;
        for (Uint i = 0; i < mProcData.CascadeCount; i++)
        {
            const float p = (i + 1) / static_cast<float>(mProcData.CascadeCount);
            const float log = minZ * glm::pow(ratio, p);
            const float uniform = minZ + range * p;
            const float d = mProcData.CascadeSplitLambda * (log - uniform) + uniform;
            mCascadeSplits[i] = (d - nearClip) / clipRange;
        }

        float lastSplitDist = 0.0f;
        // Calculate Orthographic Projection matrix for each cascade
        for (Uint cascade = 0; cascade < mProcData.CascadeCount; cascade++)
        {
            float splitDist = mCascadeSplits[cascade];
            glm::vec4 frustumCorners[NUM_FRUSTUM_CORNERS] =
                {
                    //Near face
                    {1.0f, 1.0f, -1.0f, 1.0f},
                    {-1.0f, 1.0f, -1.0f, 1.0f},
                    {1.0f, -1.0f, -1.0f, 1.0f},
                    {-1.0f, -1.0f, -1.0f, 1.0f},

                    //Far face
                    {1.0f, 1.0f, 1.0f, 1.0f},
                    {-1.0f, 1.0f, 1.0f, 1.0f},
                    {1.0f, -1.0f, 1.0f, 1.0f},
                    {-1.0f, -1.0f, 1.0f, 1.0f},
                };

            // Project frustum corners into world space from clip space
            for (glm::vec4& frustumCorner : frustumCorners)
            {
                glm::vec4 invCorner = inverseViewProjection * frustumCorner;
                frustumCorner = invCorner / invCorner.w;
            }

            for (Uint i = 0; i < 4; i++)
            {
                glm::vec4 dist = frustumCorners[i + 4] - frustumCorners[i];
                frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
                frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
            }

            // Get frustum center
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for (glm::vec4& frustumCorner : frustumCorners)
                frustumCenter += glm::vec3(frustumCorner);
            frustumCenter /= NUM_FRUSTUM_CORNERS;

            // Get the minimum and maximum extents
            float radius = 0.0f;
            for (glm::vec4& frustumCorner : frustumCorners)
            {
                float distance = glm::length(glm::vec3(frustumCorner) - frustumCenter);
                radius = glm::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;
            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            // Calculate the view and projection matrix
            glm::vec3 lightDir = -normalizedDirection;
            glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 lightProjectionMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, -15.0f, maxExtents.z - minExtents.z + 15.0f);

            // Offset to texel space to avoid shimmering ->(https://stackoverflow.com/q/33499053/14349078)
            glm::mat4 shadowMatrix = lightProjectionMatrix * lightViewMatrix;
            glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin = shadowMatrix * shadowOrigin;
            float storedW = shadowOrigin.w;
            shadowOrigin = shadowOrigin * static_cast<float>(mProcData.ShadowMapResolution) / 2.0f;
            glm::vec4 roundedOrigin = glm::round(shadowOrigin);
            glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
            roundOffset = roundOffset * 2.0f / static_cast<float>(mProcData.ShadowMapResolution);
            roundOffset.z = 0.0f;
            roundOffset.w = 0.0f;
            glm::mat4 shadowProj = lightProjectionMatrix;
            shadowProj[3] += roundOffset;
            lightProjectionMatrix = shadowProj;

            // Store SplitDistance and ViewProjection-Matrix
            mProcData.CascadeSplitDepths[cascade] = (nearClip + splitDist * clipRange) * 1.0f;
            mProcData.LightViewProjections[cascade] = lightProjectionMatrix * lightViewMatrix;

            lastSplitDist = mCascadeSplits[cascade];
        }
    }

    void ShadowMapProcedure::Update()
    {
        SURGE_PROFILE_FUNC("ShadowMapProcedure::Update");

        glm::vec3 direction;
        if (mRendererData->SceneContext)
        {
            auto view = mRendererData->SceneContext->GetRegistry().view<TransformComponent, DirectionalLightComponent>();
            for (const entt::entity& entity : view)
            {
                auto [transform, light] = view.get<TransformComponent, DirectionalLightComponent>(entity);
                direction = transform.GetTransform()[2];
            }
        }

        GeometryProcedure::InternalData* geometryProcData = Core::GetRenderer()->GetRenderProcManager()->GetRenderProcData<GeometryProcedure>();
        glm::mat4 cameraData[3] = {mRendererData->ViewMatrix, mRendererData->ProjectionMatrix};
        mRendererData->CameraUniformBuffer->SetData(cameraData);
        mRendererData->CameraDescriptorSet->SetBuffer(mRendererData->CameraUniformBuffer, 0);
        mRendererData->CameraDescriptorSet->UpdateForRendering();
        mRendererData->CameraDescriptorSet->Bind(mRendererData->RenderCmdBuffer, geometryProcData->GeometryPipeline);

        ShadowParams settings;

        // Calculate the ViewProjection matrices
        CalculateMatricesAndUpdateCBuffer(mRendererData->ViewProjection, glm::normalize(direction));
        // Loop over all the shadow maps and bind and render the whole scene to each of them
        for (Uint j = 0; j < mProcData.CascadeCount; j++)
        {
            const Ref<Framebuffer>& shadowMapBuffer = mProcData.ShadowMapFramebuffers[j];
            const Ref<GraphicsPipeline>& shadowPipeline = mProcData.ShadowMapPipelines[j];

            shadowMapBuffer->BeginRenderPass(mRendererData->RenderCmdBuffer);
            shadowPipeline->Bind(mRendererData->RenderCmdBuffer);
            settings.LightSpaceMatrix[j] = mProcData.LightViewProjections[j];

            for (const DrawCommand& drawCmd : mRendererData->DrawList)
            {
                const Ref<Mesh>& mesh = drawCmd.MeshComp->Mesh;
                const Submesh* submeshes = mesh->GetSubmeshes().data();

                mesh->GetVertexBuffer()->Bind(mRendererData->RenderCmdBuffer);
                mesh->GetIndexBuffer()->Bind(mRendererData->RenderCmdBuffer);

                for (Uint i = 0; i < mesh->GetSubmeshes().size(); i++)
                {
                    const Submesh& submesh = submeshes[i];

                    glm::mat4 meshData[2] = {drawCmd.Transform * submesh.Transform, mProcData.LightViewProjections[j]};
                    shadowPipeline->SetPushConstantData(mRendererData->RenderCmdBuffer, "uMesh", meshData);
                    shadowPipeline->DrawIndexed(mRendererData->RenderCmdBuffer, submesh.IndexCount, submesh.BaseIndex, submesh.BaseVertex);
                }
            }
            shadowMapBuffer->EndRenderPass(mRendererData->RenderCmdBuffer);
            mProcData.ShadowDesciptorSet->SetImage2D(mProcData.ShadowMapFramebuffers[j]->GetDepthAttachment(), j + 1);
        }
        mProcData.ShadowDesciptorSet->SetBuffer(mProcData.ShadowUniformBuffer, 0);

        // Update the shadow params buffer
        for (Uint i = 0; i < mProcData.CascadeCount; i++)
            settings.CascadeEnds[i] = mProcData.CascadeSplitDepths[i];
        settings.ShowCascades = mProcData.VisualizeCascades;
        mProcData.ShadowUniformBuffer->SetData(&settings);
    }

    void ShadowMapProcedure::Shutdown()
    {
    }

} // namespace Surge

// Empty Reflection, register nothing
SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::ShadowMapProcedure)
SURGE_REFLECT_CLASS_REGISTER_END(Surge::ShadowMapProcedure)