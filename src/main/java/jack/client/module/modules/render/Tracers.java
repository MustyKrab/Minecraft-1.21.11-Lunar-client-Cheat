package jack.client.module.modules.render;

import com.mojang.blaze3d.systems.RenderSystem;
import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.client.render.*;
import net.minecraft.client.util.math.MatrixStack;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Matrix4f;

public class Tracers extends Module {
    public Tracers() {
        super("Tracers", Category.RENDER, 0);
    }

    @Override
    public void onWorldRender(MatrixStack matrices, Camera camera) {
        if (mc.world == null || mc.player == null) return;

        // Use player's camera pos to avoid Camera mapping issues
        Vec3d cameraPos = mc.player.getCameraPosVec(1.0f);
        Vec3d start = new Vec3d(0, 0, 1)
                .rotateX(-mc.player.getPitch() * (float) (Math.PI / 180.0))
                .rotateY(-mc.player.getYaw() * (float) (Math.PI / 180.0))
                .add(cameraPos);

        RenderSystem.depthMask(false);
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        
        // In 1.21, it's usually getPositionColorProgram
        RenderSystem.setShader(GameRenderer::getPositionColorProgram);

        Tessellator tessellator = Tessellator.getInstance();
        // In 1.21, VertexFormat.DrawMode might be VertexFormat.DrawMode.DEBUG_LINES or just DrawMode.DEBUG_LINES
        BufferBuilder buffer = tessellator.begin(VertexFormat.DrawMode.DEBUG_LINES, VertexFormats.POSITION_COLOR);

        Matrix4f matrix = matrices.peek().getPositionMatrix();

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d ePos = entity.getLerpedPos(1.0f);

                float x1 = (float) (start.x - cameraPos.x);
                float y1 = (float) (start.y - cameraPos.y);
                float z1 = (float) (start.z - cameraPos.z);

                float x2 = (float) (ePos.x - cameraPos.x);
                float y2 = (float) (ePos.y - cameraPos.y);
                float z2 = (float) (ePos.z - cameraPos.z);

                buffer.vertex(matrix, x1, y1, z1).color(255, 0, 0, 255);
                buffer.vertex(matrix, x2, y2, z2).color(255, 0, 0, 255);
            }
        }

        // In 1.21, it's drawWithGlobalProgram
        BufferRenderer.drawWithGlobalProgram(buffer.end());
        RenderSystem.depthMask(true);
    }
}
