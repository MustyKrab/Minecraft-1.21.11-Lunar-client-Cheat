package musty.client.module;

import musty.client.module.modules.combat.Killaura;
import musty.client.module.modules.combat.Velocity;
import musty.client.module.modules.player.NoFall;
import musty.client.module.modules.render.ESP;
import musty.client.module.modules.render.GlowESP;
import musty.client.module.modules.render.HealthBars;
import musty.client.module.modules.render.SkeletonESP;
import musty.client.module.modules.render.Tracers;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.network.packet.Packet;

public class ModuleManager {
    private final List<Module> modules = new ArrayList<>();

    public void init() {
        modules.add(new ESP());
        modules.add(new GlowESP());
        modules.add(new SkeletonESP());
        modules.add(new Tracers());
        modules.add(new HealthBars());
        modules.add(new Velocity());
        modules.add(new Killaura());
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

    public void onRender(DrawContext context, float tickDelta) {
        for (Module module : modules) {
            if (module.isEnabled()) {
                module.onRender(context, tickDelta);
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
