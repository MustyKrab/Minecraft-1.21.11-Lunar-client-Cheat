package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Vector3f;

public class HealthBars extends Module {
    public HealthBars() {
        super("HealthBars", Category.RENDER, 0);
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                PlayerEntity player = (PlayerEntity) entity;
                Vec3d headPos = player.getLerpedPos(tickDelta).add(0, player.getHeight() + 0.5, 0);
                
                Vector3f projHead = project2D(headPos, tickDelta);

                if (projHead != null) {
                    float hp = player.getHealth();
                    float maxHp = player.getMaxHealth();
                    float hpPercent = hp / maxHp;
                    
                    int barWidth = 40;
                    int barHeight = 5;
                    int barX = (int)projHead.x - (barWidth / 2);
                    int barY = (int)projHead.y;
                    
                    context.fill(barX - 1, barY - 1, barX + barWidth + 1, barY + barHeight + 1, 0xFF000000);
                    context.fill(barX, barY, barX + barWidth, barY + barHeight, 0xFF444444);
                    
                    int hpColor = hpPercent > 0.5 ? 0xFF00FF00 : (hpPercent > 0.25 ? 0xFFFFFF00 : 0xFFFF0000);
                    context.fill(barX, barY, barX + (int)(barWidth * hpPercent), barY + barHeight, hpColor);
                    
                    String hpText = String.format("%.1f", hp);
                    int textWidth = mc.textRenderer.getWidth(hpText);
                    context.drawText(mc.textRenderer, hpText, (int)projHead.x - (textWidth / 2), barY - 10, 0xFFFFFFFF, true);
                }
            }
        }
    }
}
