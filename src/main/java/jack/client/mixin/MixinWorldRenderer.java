package jack.client.mixin;

import jack.client.JackClient;
import net.minecraft.client.render.Camera;
import net.minecraft.client.render.GameRenderer;
import net.minecraft.client.render.LightmapTextureManager;
import net.minecraft.client.render.WorldRenderer;
import net.minecraft.client.util.math.MatrixStack;
import org.joml.Matrix4f;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(WorldRenderer.class)
public class MixinWorldRenderer {
    @Inject(method = "render", at = @At("TAIL"))
    private void onRender(net.minecraft.client.render.RenderTickCounter tickCounter, boolean renderBlockOutline, Camera camera, GameRenderer gameRenderer, LightmapTextureManager lightmapTextureManager, Matrix4f matrix4f, Matrix4f matrix4f2, CallbackInfo ci) {
        if (JackClient.moduleManager != null) {
            // We create a dummy MatrixStack since we have the projection matrix
            MatrixStack matrices = new MatrixStack();
            matrices.multiplyPositionMatrix(matrix4f);
            JackClient.moduleManager.onWorldRender(matrices, camera);
        }
    }
}
