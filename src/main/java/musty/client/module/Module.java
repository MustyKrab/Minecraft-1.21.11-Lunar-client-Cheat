package musty.client.module;

import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.network.packet.Packet;
import net.minecraft.util.math.Vec3d;
import org.joml.Matrix4f;
import org.joml.Vector3f;
import org.joml.Vector4f;
import java.lang.reflect.Field;

public abstract class Module {
    protected static final MinecraftClient mc = MinecraftClient.getInstance();
    
    private final String name;
    private final Category category;
    private boolean enabled;
    private int keybind;

    public Module(String name, Category category, int keybind) {
        this.name = name;
        this.category = category;
        this.keybind = keybind;
    }

    public String getName() { return name; }
    public Category getCategory() { return category; }
    public int getKeybind() { return keybind; }
    public void setKeybind(int keybind) { this.keybind = keybind; }
    
    public boolean isEnabled() { return enabled; }

    public void toggle() {
        this.enabled = !this.enabled;
        if (this.enabled) {
            onEnable();
        } else {
            onDisable();
        }
    }

    public void setEnabled(boolean enabled) {
        if (this.enabled != enabled) {
            this.enabled = enabled;
            if (enabled) onEnable();
            else onDisable();
        }
    }

    protected void onEnable() {}
    protected void onDisable() {}
    
    public void onTick() {}
    public void onRender(DrawContext context, float tickDelta) {}

    public boolean onSendPacket(Packet<?> packet) { return false; }
    public boolean onReceivePacket(Packet<?> packet) { return false; }

    // True 3D to 2D HUD projection using Lunar Client's cached matrices
    protected Vector3f project2D(Vec3d pos, float tickDelta) {
        if (mc.player == null || mc.options == null || mc.getWindow() == null) return null;

        Matrix4f viewMatrix = null;
        Matrix4f projMatrix = null;

        try {
            // Read the exact matrices Lunar Client uses for rendering the world
            Field mvField = net.minecraft.client.render.GameRenderer.class.getDeclaredField("lunar$savedModelView$v1_19_3");
            mvField.setAccessible(true);
            viewMatrix = new Matrix4f((Matrix4f) mvField.get(mc.gameRenderer));

            Field pField = net.minecraft.client.render.GameRenderer.class.getDeclaredField("lunar$savedProjection$v1_19_3");
            pField.setAccessible(true);
            projMatrix = new Matrix4f((Matrix4f) pField.get(mc.gameRenderer));
        } catch (Exception e) {
            // Fallback if not on Lunar Client
            float yaw = mc.player.getYaw(tickDelta);
            float pitch = mc.player.getPitch(tickDelta);
            viewMatrix = new Matrix4f()
                .rotationX(-pitch * (float)(Math.PI / 180.0))
                .rotationY(-yaw * (float)(Math.PI / 180.0) - (float)Math.PI);
                
            float fov = (float) Math.toRadians(mc.options.getFov().getValue());
            float aspect = (float) mc.getWindow().getFramebufferWidth() / (float) mc.getWindow().getFramebufferHeight();
            projMatrix = new Matrix4f().setPerspective(fov, aspect, 0.05f, 1000.0f);
        }

        Vec3d cameraPos = null;
        try {
            // Try to get the exact camera position from the Camera object
            Field posField = net.minecraft.client.render.Camera.class.getDeclaredField("field_18712");
            posField.setAccessible(true);
            cameraPos = (Vec3d) posField.get(mc.gameRenderer.getCamera());
        } catch (Exception e) {
            try {
                Field posField2 = net.minecraft.client.render.Camera.class.getDeclaredField("pos");
                posField2.setAccessible(true);
                cameraPos = (Vec3d) posField2.get(mc.gameRenderer.getCamera());
            } catch (Exception e2) {
                cameraPos = mc.player.getCameraPosVec(tickDelta);
            }
        }

        if (cameraPos == null) return null;

        Vec3d diff = pos.subtract(cameraPos);
        Vector4f vec = new Vector4f((float)diff.x, (float)diff.y, (float)diff.z, 1.0f);
        
        viewMatrix.transform(vec);
        projMatrix.transform(vec);

        if (vec.w > 0.1f) {
            float ndcX = vec.x / vec.w;
            float ndcY = vec.y / vec.w;
            int width = mc.getWindow().getScaledWidth();
            int height = mc.getWindow().getScaledHeight();
            float screenX = (width / 2.0f) * (ndcX + 1.0f);
            float screenY = (height / 2.0f) * (1.0f - ndcY);
            return new Vector3f(screenX, screenY, vec.w);
        }
        return null;
    }

    protected void drawLine(DrawContext context, int x1, int y1, int x2, int y2, int color) {
        int dx = Math.abs(x2 - x1);
        int dy = Math.abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;
        
        int maxDots = 2000;
        int dots = 0;

        while (dots < maxDots) {
            context.fill(x1, y1, x1 + 1, y1 + 1, color);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x1 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y1 += sy;
            }
            dots++;
        }
    }
}
