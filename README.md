# BlendSpace Builder

An Unreal Engine editor plugin that automatically generates 2D locomotion blend spaces from skeleton animations.

## Features

- **Context Menu Integration**: Right-click on Skeleton or Skeletal Mesh assets to generate blend spaces
- **Automatic Animation Classification**: Uses regex patterns to identify locomotion animations (Idle, Walk, Run, Sprint in 8 directions)
- **Root Motion Priority**: Automatically prefers root motion animations when multiple candidates exist
- **Data Asset Configuration**: Customize name patterns and speed tiers via data assets
- **Modal Dialog UI**: Configure axis ranges, select animations, and set output path before generation

## Requirements

- Unreal Engine 5.x
- Editor module (not available in packaged games)

## Installation

### As Git Submodule

```bash
cd YourProject/Plugins
git submodule add https://github.com/YourUsername/BlendSpaceBuilder.git
```

### Manual Installation

1. Download or clone this repository
2. Copy to your project's `Plugins` folder
3. Regenerate project files
4. Build the project

## Usage

1. Open Content Browser
2. Right-click on a **Skeleton** or **Skeletal Mesh** asset
3. Select **"Generate Locomotion BlendSpace"**
4. In the dialog:
   - Adjust axis ranges if needed (default: -500 to 500)
   - Review and select animations for each locomotion role
   - Set output asset name
5. Click **"Create BlendSpace"**

## Configuration

### Editor Settings

Access via **Project Settings > Plugins > BlendSpace Builder**:

| Setting | Description | Default |
|---------|-------------|---------|
| Default Min Speed | Minimum axis value | -500 |
| Default Max Speed | Maximum axis value | 500 |
| X Axis Name | Horizontal axis label | RightVelocity |
| Y Axis Name | Vertical axis label | ForwardVelocity |
| Prefer Root Motion | Prioritize root motion animations | true |
| Output Asset Suffix | Suffix for generated asset name | _Locomotion |
| Default Pattern Asset | Custom pattern data asset | (none) |

### Pattern Data Asset

Create a `ULocomotionPatternDataAsset` to customize animation name matching:

**Speed Tiers** define velocity values for each movement type:
- Walk: 200
- Run: 400
- Sprint: 600

**Pattern Entries** use regex to match animation names:
```
Pattern: "idle"           → Role: Idle           (0, 0)
Pattern: "walk.*fwd"      → Role: WalkForward    (0, WalkSpeed)
Pattern: "run.*left"      → Role: RunLeft        (-RunSpeed, 0)
```

## Supported Locomotion Roles

| Role | BlendSpace Position (X, Y) |
|------|---------------------------|
| Idle | (0, 0) |
| Walk Forward | (0, WalkSpeed) |
| Walk Backward | (0, -WalkSpeed) |
| Walk Left | (-WalkSpeed, 0) |
| Walk Right | (WalkSpeed, 0) |
| Walk Forward-Left | (-WalkSpeed, WalkSpeed) |
| Walk Forward-Right | (WalkSpeed, WalkSpeed) |
| Walk Backward-Left | (-WalkSpeed, -WalkSpeed) |
| Walk Backward-Right | (WalkSpeed, -WalkSpeed) |
| Run * | Same pattern with RunSpeed |
| Sprint Forward | (0, SprintSpeed) |

## Default Pattern Matching

The plugin recognizes common animation naming conventions:

- **Idle**: `idle`
- **Direction keywords**: `fwd`, `forward`, `front`, `bwd`, `backward`, `back`, `left`, `right`, `l`, `r`
- **Diagonal shortcuts**: `fl`, `fr`, `bl`, `br`
- **Movement types**: `walk`, `run`, `sprint`
- **Root motion suffix**: `_rm` (informational, actual detection uses `bEnableRootMotion`)

Examples that will be matched:
- `AS_Character_Idle`
- `Walk_Fwd`, `WalkForward`, `walk_forward_rm`
- `Run_L`, `RunLeft`, `run_left_ip`
- `Sprint`, `SprintForward`

## Architecture

```
BlendSpaceBuilder/
├── BlendSpaceBuilder.uplugin
└── Source/BlendSpaceBuilder/
    ├── Public/
    │   ├── BlendSpaceBuilder.h              # Module class
    │   ├── BlendSpaceBuilderSettings.h      # Editor settings
    │   ├── LocomotionPatternDataAsset.h     # Pattern configuration
    │   ├── LocomotionAnimClassifier.h       # Animation classifier
    │   └── BlendSpaceFactory.h              # BlendSpace creator
    └── Private/
        ├── BlendSpaceBuilder.cpp
        ├── BlendSpaceBuilderSettings.cpp
        ├── LocomotionPatternDataAsset.cpp
        ├── LocomotionAnimClassifier.cpp
        ├── BlendSpaceFactory.cpp
        └── UI/
            ├── SBlendSpaceConfigDialog.*    # Main dialog
            └── SLocomotionAnimSelector.*    # Animation selector
```

## Future Plans

- [ ] Root motion analysis for automatic speed detection
- [ ] Auto-configure axis ranges based on analyzed speeds
- [ ] 1D BlendSpace support
- [ ] Aim Offset generation

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
