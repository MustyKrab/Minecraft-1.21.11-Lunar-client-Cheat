package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Matrix4f;
import org.joml.Vector4f;

public class Tracers extends Module {
    public Tracers() {
        super("Tracers", Category.RENDER, 0);
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        int width = mc.getWindow().getScaledWidth();
        int height = mc.getWindow().getScaledHeight();
        
        Vec3d cameraPos = mc.player.getCameraPosVec(tickDelta);
        float yaw = mc.player.getYaw(tickDelta);
        float pitch = mc.player.getPitch(tickDelta);
        
        // Build View Matrix
        Matrix4f viewMatrix = new Matrix4f()
            .rotationX(-pitch * (float)(Math.PI / 180.0))
            .rotationY(-yaw * (float)(Math.PI / 180.0) - (float)Math.PI);
            
        // Build Projection Matrix
        float fov = (float) Math.toRadians(mc.options.getFov().getValue());
        float aspect = (float) mc.getWindow().getFramebufferWidth() / (float) mc.getWindow().getFramebufferHeight();
        Matrix4f projMatrix = new Matrix4f().setPerspective(fov, aspect, 0.05f, 1000.0f);

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d entityPos = entity.getLerpedPos(tickDelta);
                Vec3d diff = entityPos.subtract(cameraPos);
                
                Vector4f pos = new Vector4f((float)diff.x, (float)diff.y, (float)diff.z, 1.0f);
                
                // Project 3D to 2D
                viewMatrix.transform(pos);
                projMatrix.transform(pos);
                
                // If w > 0, the entity is in front of the camera
                if (pos.w > 0.0f) {
                    float ndcX = pos.x / pos.w;
                    float ndcY = pos.y / pos.w;
                    
                    int screenX = (int) ((ndcX + 1.0f) * 0.5f * width);
                    int screenY = (int) ((1.0f - ndcY) * 0.5f * height);
                    
                    int startX = width / 2;
                    int startY = height;
                    
                    drawLine(context, startX, startY, screenX, screenY, 0xFFFF0000);
                }
            }
        }
    }
    
    private void drawLine(DrawContext context, int x1, int y1, int x2, int y2, int color) {
        int dx = Math.abs(x2 - x1);
        int dy = Math.abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;
        
        int maxDots = 1000;
        int dots = 0;

        while (dots < maxDots) {
            context.fill(x1, y1, x1 + 1, y1 + 1, color);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x1 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y1 += sy;
            }
            dots++;
        }
    }
}
