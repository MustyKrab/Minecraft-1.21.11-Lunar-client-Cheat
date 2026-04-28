package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.LivingEntity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Vector3f;

public class Tracers extends Module {
    public Tracers() {
        super("Tracers", Category.RENDER, 0);
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        int width = mc.getWindow().getScaledWidth();
        int height = mc.getWindow().getScaledHeight();
        
        Vec3d cameraPos = mc.gameRenderer.getCamera().getPos();

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d entityPos = entity.getLerpedPos(tickDelta);
                
                // Simple world to screen using crosshair as origin for tracers
                // A proper 3D to 2D projection requires matrix math, but for a simple 2D HUD overlay,
                // we can approximate or use the center of the screen to the projected point.
                // Since drawing lines in DrawContext is limited, we'll draw a simple line from center bottom.
                
                // Note: Proper 3D tracers require OpenGL/Matrix manipulation which is better done in WorldRenderer mixin.
                // For this HUD mixin, we'll just draw a line from crosshair to the entity's approximate screen pos if it's in front.
                
                Vec3d diff = entityPos.subtract(cameraPos);
                Vec3d look = mc.player.getRotationVec(tickDelta);
                
                // Dot product to check if entity is in front
                if (diff.normalize().dotProduct(look) > 0) {
                    // Very rough approximation for HUD drawing
                    double yawDiff = Math.toDegrees(Math.atan2(diff.z, diff.x)) - 90 - mc.player.getYaw(tickDelta);
                    double pitchDiff = Math.toDegrees(Math.atan2(-diff.y, Math.sqrt(diff.x*diff.x + diff.z*diff.z))) - mc.player.getPitch(tickDelta);
                    
                    // Wrap angles
                    while (yawDiff <= -180) yawDiff += 360;
                    while (yawDiff > 180) yawDiff -= 360;
                    
                    int screenX = width / 2 + (int)(yawDiff * 10);
                    int screenY = height / 2 + (int)(pitchDiff * 10);
                    
                    // Draw line from center to screenX, screenY
                    // DrawContext doesn't have a direct drawLine, so we fill a thin rect or use a custom vertex builder
                    // For simplicity in this HUD context, we'll just draw a small dot at the target
                    context.fill(screenX - 1, screenY - 1, screenX + 1, screenY + 1, 0xFFFF0000);
                    
                    // To draw an actual line, we'd need to use Tessellator.
                }
            }
        }
    }
}
