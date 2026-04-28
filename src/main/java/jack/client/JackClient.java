package jack.client;

import jack.client.gui.ClickGUI;
import jack.client.module.ModuleManager;
import jack.client.module.modules.render.HealthBars;
import jack.client.module.modules.render.Tracers;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.rendering.v1.HudRenderCallback;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.entity.Entity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.math.Vec3d;
import org.joml.Matrix4f;
import org.joml.Vector4f;
import org.lwjgl.glfw.GLFW;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class JackClient implements ModInitializer {
    public static final String MOD_ID = "jackclient";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);
    
    public static ModuleManager moduleManager;
    private boolean wasPressed = false;

    private static Matrix4f lastProjMat = new Matrix4f();
    private static Matrix4f lastModMat = new Matrix4f();
    private static Vec3d lastCamPos = Vec3d.ZERO;

    @Override
    public void onInitialize() {
        LOGGER.info("Jack Client Initializing...");
        
        moduleManager = new ModuleManager();
        moduleManager.init();

        ClientTickEvents.END_CLIENT_TICK.register(client -> {
            if (client.getWindow() != null) {
                boolean isPressed = GLFW.glfwGetKey(client.getWindow().getHandle(), GLFW.GLFW_KEY_LEFT_BRACKET) == GLFW.GLFW_PRESS;
                if (isPressed && !wasPressed) {
                    if (client.currentScreen == null) {
                        client.setScreen(new ClickGUI());
                    } else if (client.currentScreen instanceof ClickGUI) {
                        client.setScreen(null);
                    }
                }
                wasPressed = isPressed;
            }
        });

        WorldRenderEvents.LAST.register(ctx -> {
            lastProjMat.set(ctx.projectionMatrix());
            lastModMat.set(ctx.matrixStack().peek().getPositionMatrix());
            lastCamPos = ctx.camera().getPos();
        });

        HudRenderCallback.EVENT.register((context, tickCounter) -> {
            Tracers tracers = (Tracers) moduleManager.getModuleByName("Tracers");
            HealthBars healthBars = (HealthBars) moduleManager.getModuleByName("HealthBars");
            
            if ((tracers != null && tracers.isEnabled()) || (healthBars != null && healthBars.isEnabled())) {
                render2DESO(context, tracers != null && tracers.isEnabled(), healthBars != null && healthBars.isEnabled());
            }
        });
        
        LOGGER.info("Jack Client Initialized!");
    }

    private static void render2DESP(DrawContext context, boolean drawTracers, boolean drawHealth) {
        MinecraftClient mc = MinecraftClient.getInstance();
        if (mc.world == null || mc.player == null) return;
        
        int width = mc.getWindow().getScaledWidth();
        int height = mc.getWindow().getScaledHeight();
        
        for (Entity entity : mc.world.getEntities()) {
            if (entity == mc.player || !(entity instanceof PlayerEntity)) continue;
            
            double x = entity.getX() - lastCamPos.x;
            double y = entity.getY() - lastCamPos.y;
            double z = entity.getZ() - lastCamPos.z;
            
            Vector4f bottom = new Vector4f((float)x, (float)y, (float)z, 1.0f);
            bottom.mul(lastModMat);
            bottom.mul(lastProjMat);
            
            Vector4f top = new Vector4f((float)x, (float)y + entity.getHeight(), (float)z, 1.0f);
            top.mul(lastModMat);
            top.mul(lastProjMat);
            
            if (bottom.w <= 0.1f || top.w <= 0.1f) continue;
            
            float ndcBottomX = bottom.x / bottom.w;
            float ndcBottomY = bottom.y / bottom.w;
            float ndcTopX = top.x / top.w;
            float ndcTopY = top.y / top.w;
            
            int screenBottomX = (int)((width / 2.0f) * (ndcBottomX + 1.0f));
            int screenBottomY = (int)((height / 2.0f) * (1.0f - ndcBottomY));
            
            int screenTopX = (int)((width / 2.0f) * (ndcTopX + 1.0f));
            int screenTopY = (int)((height / 2.0f) * (1.0f - ndcTopY));
            
            int boxHeight = screenBottomY - screenTopY;
            int boxWidth = boxHeight / 2;
            int boxLeft = screenTopX - boxWidth / 2;
            
            if (drawTracers) {
                // Draw line from center of screen to bottom of entity
                drawLine(context, width / 2, height / 2, screenBottomX, screenBottomY, 0xFFFFFFFF);
            }
            
            if (drawHealth) {
                float hp = ((PlayerEntity)entity).getHealth();
                float maxHp = ((PlayerEntity)entity).getMaxHealth();
                float hpPercent = Math.max(0, Math.min(1, hp / maxHp));
                
                int barHeight = (int)(boxHeight * hpPercent);
                int barX = boxLeft - 4;
                
                // Background
                context.fill(barX, screenTopY, barX + 2, screenBottomY, 0xFF000000);
                // Health
                int hpColor = hpPercent > 0.5f ? 0xFF00FF00 : (hpPercent > 0.25f ? 0xFFFFF00 : 0xFFFF0000);
                context.fill(barX, screenBottomY - barHeight, barX + 2, screenBottomY, hpColor);
            }
        }
    }

    private static void drawLine(DrawContext context, int x1, int y1, int x2, int y2, int color) {
        int dx = Math.abs(x2 - x1);
        int dy = Math.abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;
        
        while (true) {
            context.fill(x1, y1, x1 + 1, y1 + 1, color);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x1 += sx; }
            if (e2 < dx) { err += dx; y1 += sy; }
        }
    }
}
