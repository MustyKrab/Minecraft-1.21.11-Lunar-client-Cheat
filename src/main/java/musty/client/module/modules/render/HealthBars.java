package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
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
        
        // Better pixel scaling based on vertical FOV
        float fov = mc.options.getFov().getValue().floatValue();
        float pixelsPerDegree = height / fov;

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                PlayerEntity player = (PlayerEntity) entity;
                
                // Get the position slightly above the player's head
                Vec3d headPos = player.getLerpedPos(tickDelta).add(0, player.getHeight() + 0.3, 0);
                Vec3d diff = headPos.subtract(cameraPos);
                
                double diffXZ = Math.sqrt(diff.x * diff.x + diff.z * diff.z);
                float yawToEntity = (float) Math.toDegrees(Math.atan2(diff.z, diff.x)) - 90.0f;
                float pitchToEntity = (float) -Math.toDegrees(Math.atan2(diff.y, diffXZ));
                
                float relativeYaw = wrapDegrees(yawToEntity - yaw);
                float relativePitch = wrapDegrees(pitchToEntity - pitch);
                
                // Only draw if entity is in front of the camera
                if (Math.abs(relativeYaw) < 90.0f) {
                    int screenX = (width / 2) + (int)(relativeYaw * pixelsPerDegree);
                    int screenY = (height / 2) + (int)(relativePitch * pixelsPerDegree);
                    
                    float hp = player.getHealth();
                    float maxHp = player.getMaxHealth();
                    float hpPercent = hp / maxHp;
                    
                    int barWidth = 40;
                    int barHeight = 5;
                    int barX = screenX - (barWidth / 2);
                    int barY = screenY; // Draw exactly at the projected head pos
                    
                    // Draw background (black border)
                    context.fill(barX - 1, barY - 1, barX + barWidth + 1, barY + barHeight + 1, 0xFF000000);
                    // Draw background (dark gray inner)
                    context.fill(barX, barY, barX + barWidth, barY + barHeight, 0xFF444444);
                    
                    // Draw health
                    int hpColor = hpPercent > 0.5 ? 0xFF00FF00 : (hpPercent > 0.25 ? 0xFFFFFF00 : 0xFFFF0000);
                    context.fill(barX, barY, barX + (int)(barWidth * hpPercent), barY + barHeight, hpColor);
                    
                    // Draw HP text
                    String hpText = String.format("%.1f", hp);
                    int textWidth = mc.textRenderer.getWidth(hpText);
                    context.drawText(mc.textRenderer, hpText, screenX - (textWidth / 2), barY - 10, 0xFFFFFFFF, true);
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
