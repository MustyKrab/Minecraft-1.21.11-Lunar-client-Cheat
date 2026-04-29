package musty.client;

import musty.client.gui.ClickGUI;
import musty.client.module.ModuleManager;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.util.InputUtil;
import org.lwjgl.glfw.GLFW;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MustyClient implements ModInitializer {
    public static final String MOD_ID = "mustyclient";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);
    
    public static ModuleManager moduleManager;
    private boolean wasPressed = false;

    @Override
    public void onInitialize() {
        LOGGER.info("Musty Client Initializing...");
        
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
        
        LOGGER.info("Musty Client Initialized!");
    }
}
