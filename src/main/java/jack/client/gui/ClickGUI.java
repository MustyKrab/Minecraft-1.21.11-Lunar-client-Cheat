package jack.client.gui;

import jack.client.JackClient;
import jack.client.module.Module;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.text.Text;

public class ClickGUI extends Screen {
    private int x = 100;
    private int y = 100;
    private int width = 400;
    private int height = 300;
    private boolean dragging = false;
    private int dragX, dragY;

    public ClickGUI() {
        super(Text.literal("Jack Client GUI"));
    }

    @Override
    protected void init() {
        super.init();
    }

    @Override
    public void render(DrawContext context, int mouseX, int mouseY, float delta) {
        // Draw background
        context.fill(x, y, x + width, y + height, 0xAA000000);
        
        // Draw header
        context.fill(x, y, x + width, y + 20, 0xFF333333);
        context.drawText(client.textRenderer, "Jack Client 1.21.11", x + 5, y + 6, 0xFFFFFFFF, true);

        // Draw Modules
        int moduleY = y + 25;
        for (Module module : JackClient.moduleManager.getModules()) {
            int color = module.isEnabled() ? 0xFF00FF00 : 0xFFFF0000;
            context.drawText(client.textRenderer, module.getName(), x + 10, moduleY, color, true);
            moduleY += 15;
        }
    }

    public boolean mouseClicked(double mouseX, double mouseY, int button) {
        if (button == 0) { // Left click
            // Header drag check
            if (mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + 20) {
                dragging = true;
                dragX = (int) (mouseX - x);
                dragY = (int) (mouseY - y);
                return true;
            }

            // Module toggle check
            int moduleY = y + 25;
            for (Module module : JackClient.moduleManager.getModules()) {
                if (mouseX >= x + 10 && mouseX <= x + 100 && mouseY >= moduleY && mouseY <= moduleY + 10) {
                    module.toggle();
                    return true;
                }
                moduleY += 15;
            }
        }
        return false;
    }

    public boolean mouseReleased(double mouseX, double mouseY, int button) {
        if (button == 0) {
            dragging = false;
        }
        return false;
    }

    public boolean mouseDragged(double mouseX, double mouseY, int button, double deltaX, double deltaY) {
        if (dragging) {
            x = (int) (mouseX - dragX);
            y = (int) (mouseY - dragY);
            return true;
        }
        return false;
    }

    @Override
    public boolean shouldPause() {
        return false;
    }
}
