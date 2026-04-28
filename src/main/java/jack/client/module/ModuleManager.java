package jack.client.module;

import jack.client.module.modules.combat.Killaura;
import jack.client.module.modules.combat.Velocity;
import jack.client.module.modules.player.NoFall;
import net.minecraft.network.packet.Packet;
import java.util.ArrayList;
import java.util.List;

public class ModuleManager {
    private final List<Module> modules = new ArrayList<>();

    public void init() {
        // Combat
        modules.add(new Velocity());
        modules.add(new Killaura());
        
        // Player
        modules.add(new NoFall());
    }

    public List<Module> getModules() {
        return modules;
    }

    public Module getModuleByName(String name) {
        for (Module module : modules) {
            if (module.getName().equalsIgnoreCase(name)) {
                return module;
            }
        }
        return null;
    }
    
    public void onTick() {
        for (Module module : modules) {
            if (module.isEnabled()) {
                module.onTick();
            }
        }
    }

    public boolean onSendPacket(Packet<?> packet) {
        for (Module module : modules) {
            if (module.isEnabled()) {
                if (module.onSendPacket(packet)) {
                    return true;
                }
            }
        }
        return false;
    }

    public boolean onReceivePacket(Packet<?> packet) {
        for (Module module : modules) {
            if (module.isEnabled()) {
                if (module.onReceivePacket(packet)) {
                    return true;
                }
            }
        }
        return false;
    }
}
