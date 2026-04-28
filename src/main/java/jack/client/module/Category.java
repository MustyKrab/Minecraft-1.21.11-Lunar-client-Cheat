package jack.client.module;

public enum Category {
    COMBAT("Combat"),
    MOVEMENT("Movement"),
    RENDER("Render"),
    PLAYER("Player"),
    EXPLOIT("Exploit");

    public final String name;

    Category(String name) {
        this.name = name;
    }
}
