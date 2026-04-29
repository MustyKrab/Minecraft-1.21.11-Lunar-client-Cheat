package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Matrix4f;
import org.joml.Vector4f;

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
                PlayerEntity player = (PlayerEntity) entity;
                
                // Position slightly above the player's head
                Vec3d headPos = player.getLerpedPos(tickDelta).add(0, player.getHeight() + 0.3, 0);
                Vec3d diff = headPos.subtract(cameraPos);
                
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
                    
                    float hp = player.getHealth();
                    float maxHp = player.getMaxHealth();
                    float hpPercent = hp / maxHp;
                    
                    int barWidth = 40;
                    int barHeight = 5;
                    int barX = screenX - (barWidth / 2);
                    int barY = screenY;
                    
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
}
