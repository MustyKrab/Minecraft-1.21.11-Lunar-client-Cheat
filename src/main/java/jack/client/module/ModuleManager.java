package jack.client.module;

import jack.client.module.modules.combat.Killaura;
import jack.client.module.modules.combat.Velocity;
import jack.client.module.modules.player.NoFall;
import jack.client.module.modules.render.GlowESP;
import jack.client.module.modules.render.HealthBars;
import java.util.ArrayList;
import java.util.List;
import jack.client.module.modules.render.Tracers;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.render.Camera;
import net.minecraft.client.util.math.MatrixStack;
import net.minecraft.network.packet.Packet;

public class ModuleManager {
    private final List<Module> modules = new ArrayList<>();

    public void init() {
        // Render
        modules.add(new GlowESP());
        modules.add(new Tracers());
        modules.add(new HealthBars());
        
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

    public void onRender(DrawContext context, float tickDelta) {
        for (Module module : modules) {
            if (module.isEnabled()) {
                module.onRender(context, tickDelta);
            }
        }
    }
    
    public void onWorldRender(MatrixStack matrices, Camera camera) {
        for (Module module : modules) {
            if (module.isEnabled()) {
                module.onWorldRender(matrices, camera);
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
