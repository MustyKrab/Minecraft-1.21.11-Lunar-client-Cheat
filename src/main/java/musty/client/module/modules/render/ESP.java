package musty.client.module.modules.render;

import musty.client.module.Category;
import musty.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Vector3f;

public class ESP extends Module {
    public static ESP instance;

    public ESP() {
        super("ESP", Category.RENDER, 0);
        instance = this;
    }

    @Override
    public void onRender(DrawContext context, float tickDelta) {
        if (mc.world == null || mc.player == null) return;

        for (Entity entity : mc.world.getEntities()) {
            if (entity instanceof PlayerEntity && entity != mc.player) {
                Vec3d feetPos = entity.getLerpedPos(tickDelta);
                Vec3d headPos = feetPos.add(0, entity.getHeight() + 0.2, 0);

                Vector3f projFeet = project2D(feetPos, tickDelta);
                Vector3f projHead = project2D(headPos, tickDelta);

                if (projFeet != null && projHead != null) {
                    float boxHeight = projFeet.y - projHead.y;
                    float boxWidth = boxHeight / 2.0f;
                    float boxX = projHead.x - (boxWidth / 2.0f);
                    float boxY = projHead.y;

                    // Draw filled box (semi-transparent black)
                    context.fill((int)boxX, (int)boxY, (int)(boxX + boxWidth), (int)(boxY + boxHeight), 0x40000000);
                    
                    // Draw outline manually since drawBorder might not exist in this mapping version
                    int color = 0xFFFFFFFF;
                    int x = (int)boxX;
                    int y = (int)boxY;
                    int w = (int)boxWidth;
                    int h = (int)boxHeight;
                    
                    // Top
                    context.fill(x, y, x + w, y + 1, color);
                    // Bottom
                    context.fill(x, y + h - 1, x + w, y + h, color);
                    // Left
                    context.fill(x, y, x + 1, y + h, color);
                    // Right
                    context.fill(x + w - 1, y, x + w, y + h, color);

                    // Draw Nametag & Distance
                    String name = entity.getName().getString();
                    String dist = String.format("%.1fm", mc.player.distanceTo(entity));
                    String text = name + " [" + dist + "]";
                    
                    int textWidth = mc.textRenderer.getWidth(text);
                    context.drawText(mc.textRenderer, text, (int)(projHead.x - textWidth / 2), (int)boxY - 10, 0xFFFFFFFF, true);
                }
            }
        }
    }
}
