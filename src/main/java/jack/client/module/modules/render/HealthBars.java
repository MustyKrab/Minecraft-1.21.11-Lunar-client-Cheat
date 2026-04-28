package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;

public class HealthBars extends Module {
    public HealthBars() {
        super("HealthBars", Category.RENDER, 0);
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
                PlayerEntity player = (PlayerEntity) entity;
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
                    float fov = mc.options.getFov().getValue().floatValue();
                    float pixelsPerDegree = (width / fov);
                    
                    int screenX = (width / 2) + (int)(relativeYaw * pixelsPerDegree);
                    int screenY = (height / 2) + (int)(relativePitch * pixelsPerDegree);
                    
                    float hp = player.getHealth();
                    float maxHp = player.getMaxHealth();
                    float hpPercent = hp / maxHp;
                    
                    int barWidth = 30;
                    int barHeight = 4;
                    int barX = screenX - (barWidth / 2);
                    int barY = screenY - 20; // Draw above the entity center
                    
                    // Draw background
                    context.fill(barX, barY, barX + barWidth, barY + barHeight, 0xFF000000);
                    
                    // Draw health
                    int hpColor = hpPercent > 0.5 ? 0xFF00FF00 : (hpPercent > 0.25 ? 0xFFFFFF00 : 0xFFFF0000);
                    context.fill(barX + 1, barY + 1, barX + 1 + (int)((barWidth - 2) * hpPercent), barY + barHeight - 1, hpColor);
                }
            }
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
