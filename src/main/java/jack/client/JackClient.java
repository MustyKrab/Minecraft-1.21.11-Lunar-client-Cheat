package jack.client;

import jack.client.module.ModuleManager;
import net.fabricmc.api.ModInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class JackClient implements ModInitializer {
    public static final String MOD_ID = "jackclient";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);
    
    public static ModuleManager moduleManager;

    @Override
    public void onInitialize() {
        LOGGER.info("Jack Client Initializing...");
        
        moduleManager = new ModuleManager();
        moduleManager.init();
        
        LOGGER.info("Jack Client Initialized!");
    }
}
