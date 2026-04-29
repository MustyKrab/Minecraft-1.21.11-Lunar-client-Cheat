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

                    // 1. Semi-Transparent Box Fill (Matches main.cpp: Color(40, 0, 0, 0) -> 0x28000000)
                    context.fill((int)boxX, (int)boxY, (int)(boxX + boxWidth), (int)(boxY + boxHeight), 0x28000000);
                    
                    // 2. Smooth White Outline (Matches main.cpp: Color(255, 255, 255, 255) -> 0xFFFFFFFF)
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

                    // 3. Nametag & Distance Text
                    String name = entity.getName().getString();
                    String dist = String.format("%.1fm", mc.player.distanceTo(entity));
                    String text = name + " [" + dist + "]";
                    
                    int textWidth = mc.textRenderer.getWidth(text);
                    
                    // Draw text shadow for readability (Matches main.cpp offset)
                    context.drawText(mc.textRenderer, text, (int)(projHead.x - textWidth / 2) + 1, (int)boxY - 10 + 1, 0xFF000000, false);
                    // Draw main text
                    context.drawText(mc.textRenderer, text, (int)(projHead.x - textWidth / 2), (int)boxY - 10, 0xFFFFFFFF, false);
                }
            }
        }
    }
}
