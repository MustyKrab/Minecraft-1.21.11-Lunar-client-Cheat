package musty.client.gui;

import musty.client.MustyClient;
import musty.client.module.Module;
import musty.client.module.modules.combat.Killaura;
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
    private boolean draggingSlider = false;

    public ClickGUI() {
        super(Text.literal("Musty Client GUI"));
    }

    @Override
    protected void init() {
        super.init();
    }

    @Override
    public void render(DrawContext context, int mouseX, int mouseY, float delta) {
        boolean isClicked = GLFW.glfwGetMouseButton(client.getWindow().getHandle(), GLFW.GLFW_MOUSE_BUTTON_LEFT) == GLFW.GLFW_PRESS;
        
        Killaura killaura = (Killaura) MustyClient.moduleManager.getModuleByName("Killaura");
        int sliderX = x + 200;
        int sliderY = y + 30;
        int sliderWidth = 150;
        int sliderHeight = 10;

        if (isClicked && !wasClicked) {
            if (mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 20) {
                dragging = true;
                dragX = mouseX - x;
                dragY = mouseY - y;
            } else if (mouseX >= sliderX && mouseX <= sliderX + sliderWidth && mouseY >= sliderY && mouseY <= sliderY + sliderHeight) {
                draggingSlider = true;
            } else {
                int moduleY = y + 25;
                for (Module module : MustyClient.moduleManager.getModules()) {
                    if (mouseX >= x + 10 && mouseX <= x + 100 && mouseY >= moduleY && mouseY <= moduleY + 10) {
                        module.toggle();
                    }
                    moduleY += 15;
                }
            }
        } else if (!isClicked) {
            dragging = false;
            draggingSlider = false;
        }

        if (dragging) {
            x = mouseX - dragX;
            y = mouseY - dragY;
        }

        if (draggingSlider && killaura != null) {
            float pick = (float) (mouseX - sliderX) / (float) sliderWidth;
            if (pick < 0.0f) pick = 0.0f;
            if (pick > 1.0f) pick = 1.0f;
            float newReach = 3.0f + (pick * 3.0f);
            killaura.setReach(newReach);
        }

        wasClicked = isClicked;

        context.fill(x, y, x + width, y + height, 0xAA000000);
        context.fill(x, y, x + width, y + 20, 0xFF333333);
        context.drawText(client.textRenderer, "Musty Client 1.21.11", x + 5, y + 6, 0xFFFFFFFF, true);

        int moduleY = y + 25;
        for (Module module : MustyClient.moduleManager.getModules()) {
            int color = module.isEnabled() ? 0xFF00FF00 : 0xFFFF0000;
            context.drawText(client.textRenderer, module.getName(), x + 10, moduleY, color, true);
            moduleY += 15;
        }

        if (killaura != null) {
            context.fill(sliderX, sliderY, sliderX + sliderWidth, sliderY + sliderHeight, 0xFF555555);
            float pick = (killaura.getReach() -  3.0f) / 3.0f;
            context.fill(sliderX, sliderY, sliderX + (int)(sliderWidth * pick), sliderY + sliderHeight, 0xFF00FF00);
            context.drawText(client.textRenderer, "Killaura Reach: " + String.format("%.2f", killaura.getReach()), sliderX, sliderY - 10, 0xFFFFFFFF, true);
        }
    }

    @Override
    public boolean shouldPause() {
        return false;
    }
}
