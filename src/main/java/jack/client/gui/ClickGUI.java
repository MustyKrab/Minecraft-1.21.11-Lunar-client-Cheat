package jack.client.gui;

import jack.client.JackClient;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.text.Text;
import org.lwjgl.glfw.GLFW;

public class ClickGUI extends Screen {
    private int x = 100;
    private int y = 100;
    private int width = 400;
    private int height = 300;
    private boolean dragging = false;
    private int dragX, dragY;
    private boolean wasClicked = false;

    public ClickGUI() {
        super(Text.literal("Jack Client GUI"));
    }

    @Override
    protected void init() {
        super.init();
    }

    @Override
    public void render(DrawContext context, int mouseX, int mouseY, float delta) {
        // Handle mouse input manually to avoid Version-specific Screen method changes
        boolean isClicked = GLFW.glfwGetMouseButton(client.getWindow().getHandle(), GLFW.GLFW_MOUSE_BUTTON_LEFT) == GLFW.GLFW_PRESS;
        
        if (isClicked && !wasClicked) {
            // On Initial Click
            if (mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 20) {
                dragging = true;
                dragX = mouseX - x;
                dragY = mouseY - y;
            } else {
                int moduleY = y + 25;
                for (Module module : JackClient.moduleManager.getModules()) {
                    if (mouseX >= x + 10 && mouseX <= x + 100 && mouseY >= moduleY && mouseY <= moduleY + 10) {
                        module.toggle();
                    }
                    moduleY += 15;
                }
            }
        } else if (!isClicked) {
            dragging = false;
        }

        if (dragging) {
            x = mouseX - dragX;
            y = mouseY - dragY;
        }

        wasClicked = isClicked;

        // Draw background
        context.fill(x, y, x + width, y + height, 0xAA<<24 | 0x000000); // Argb black
        
        // Draw header
        context.fill(x, y, x + width, y + 20, 0xFF<<24 | 0x333333);
        context.drawText(client.textRenderer, "Jack Client 1.21.11", x + 5, y + 6, 0xFFFFFFFF, true);

        // Draw Modules
        int moduleY = y + 25;
        for (Module module : JackClient.moduleManager.getModules()) {
            int color = module.isEnabled() ? 0xFF00FF00 : 0xFFFF0000;
            context.drawText(client.textRenderer, module.getName(), x + 10, moduleY, color, true);
            moduleY += 15;
        }
    }

    @Override
    public boolean shouldPause() {
        return false;
    }
}
