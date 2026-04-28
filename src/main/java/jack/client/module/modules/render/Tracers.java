package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;

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

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d entityPos = entity.getLerpedPos(tickDelta);
                
                Vec3d diff = entityPos.subtract(cameraPos);
                
                // Calculate yaw and pitch to the entity
                double diffXZ = Math.sqrt(diff.x * diff.x + diff.z * diff.z);
                float yawToEntity = (float) Math.toDegrees(Math.atan2(diff.z, diff.x)) - 90.0f;
                float pitchToEntity = (float) -Math.toDegrees(Math.atan2(diff.y, diffXZ));
                
                // Calculate relative angles
                float relativeYaw = wrapDegrees(yawToEntity - yaw);
                float relativePitch = wrapDegrees(pitchToEntity - pitch);
                
                // Only draw if entity is somewhat in front of us (within 90 degrees)
                if (Math.abs(relativeYaw) < 90.0f) {
                    // Simple projection: map degrees to screen pixels
                    // Assuming roughly 90 FOV for simplicity
                    float fov = mc.options.getFov().getValue().floatValue();
                    float pixelsPerDegree = (width / fov);
                    
                    int screenX = (width / 2) + (int)(relativeYaw * pixelsPerDegree);
                    int screenY = (height / 2) + (int)(relativePitch * pixelsPerDegree);
                    
                    // Draw a line from bottom center of screen to the entity
                    int startX = width / 2;
                    int startY = height;
                    
                    // Since DrawContext doesn't have drawLine, we draw a series of dots
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
        
        // Limit the number of dots to prevent lag if line is too long
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
    
    private float wrapDegrees(float degrees) {
        float f = degrees % 360.0F;
        if (f >= 180.0F) {
            f -= 360.0F;
        }
        if (f < -180.0F) {
            f += 360.0F;
        }
        return f;
    }
}
