package jack.client.module.modules.render;

import jack.client.module.Category;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.LivingEntity;
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
        
        Vec3d cameraPos = mc.gameRenderer.getCamera().getPos();

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                PlayerEntity player = (PlayerEntity) entity;
                Vec3d entityPos = entity.getLerpedPos(tickDelta);
                
                Vec3d diff = entityPos.subtract(cameraPos);
                Vec3d look = mc.player.getRotationVec(tickDelta);
                
                if (diff.normalize().dotProduct(look) > 0) {
                    double yawDiff = Math.toDegrees(Math.atan2(diff.z, diff.x)) - 90 - mc.player.getYaw(tickDelta);
                    double pitchDiff = Math.toDegrees(Math.atan2(-diff.y, Math.sqrt(diff.x*diff.x + diff.z*diff.z))) - mc.player.getPitch(tickDelta);
                    
                    while (yawDiff <= -180) yawDiff += 360;
                    while (yawDiff > 180) yawDiff -= 360;
                    
                    int screenX = width / 2 + (int)(yawDiff * 10);
                    int screenY = height / 2 + (int)(pitchDiff * 10);
                    
                    float hp = player.getHealth();
                    float maxHp = player.getMaxHealth();
                    float hpPercent = hp / maxHp;
                    
                    int barWidth = 30;
                    int barHeight = 4;
                    int barX = screenX - (barWidth / 2);
                    int barY = screenY - 20; // Draw above the "tracer" dot
                    
                    // Draw background
                    context.fill(barX, barY, barX + barWidth, barY + barHeight, 0xFF000000);
                    
                    // Draw health
                    int hpColor = hpPercent > 0.5 ? 0xFF00FF00 : (hpPercent > 0.25 ? 0xFFFFFF00 : 0xFFFF0000);
                    context.fill(barX + 1, barY + 1, barX + 1 + (int)((barWidth - 2) * hpPercent), barY + barHeight - 1, hpColor);
                }
            }
        }
    }
}
