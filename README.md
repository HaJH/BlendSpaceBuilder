# BlendSpace Builder

An Unreal Engine editor plugin that automatically generates 2D locomotion blend spaces from skeleton animations with velocity analysis.

## Features

- **Context Menu Integration**: Right-click on Skeleton or Skeletal Mesh assets to generate blend spaces
- **Automatic Animation Classification**: Uses regex patterns to identify locomotion animations (Idle, Walk, Run, Sprint in 8 directions)
- **Velocity Analysis**: Two analysis modes for automatic sample positioning:
  - **Root Motion**: Calculates velocity from root motion translation
  - **Locomotion**: Analyzes foot bone movement for in-place animations
- **Automatic Axis Range**: Calculates optimal axis ranges based on analyzed velocities
- **Grid Configuration**: Customize grid divisions, snap-to-grid, and nice number rounding
- **Root Motion Priority**: Automatically prefers root motion animations when multiple candidates exist
- **Foot Bone Detection**: Auto-detects left/right foot bones from skeleton

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
   - Select animations for each locomotion role
   - Choose analysis type (Root Motion or Locomotion)
   - Click **"Analyze Samples"** to calculate velocities
   - Adjust grid settings (divisions, snap, nice numbers)
   - Review calculated axis ranges
   - Set output asset name
5. Click **"Create BlendSpace"**

## Analysis Modes

### Root Motion Analysis
Extracts velocity from the animation's root motion data. Best for animations that have root motion enabled.

### Locomotion Analysis
Calculates character velocity by analyzing foot bone movement during ground contact phases. Useful for in-place animations without root motion.

The plugin automatically detects foot bones using common naming patterns:
- Left foot: `foot_l`, `l_foot`, `leftfoot`, `left_foot`
- Right foot: `foot_r`, `r_foot`, `rightfoot`, `right_foot`

## Grid Configuration

| Setting | Description | Default |
|---------|-------------|---------|
| Grid Divisions | Number of grid divisions (1-16) | 4 |
| Snap to Grid | Snap samples to grid points | true |
| Use Nice Numbers | Round axis range to nice values (10, 25, 50, 100...) | true |

**Nice Numbers Example** (max velocity = 110, divisions = 4):
- With Nice Numbers: Range ±200, Step = 100
- Without Nice Numbers: Range ±122, Step = 61

## Editor Settings

Access via **Project Settings > Plugins > BlendSpace Builder**:

| Setting | Description | Default |
|---------|-------------|---------|
| Default Min Speed | Minimum axis value | -500 |
| Default Max Speed | Maximum axis value | 500 |
| X Axis Name | Horizontal axis label | RightVelocity |
| Y Axis Name | Vertical axis label | ForwardVelocity |
| Prefer Root Motion | Prioritize root motion animations | true |
| Output Asset Suffix | Suffix for generated asset name | _Locomotion |
| Left Foot Patterns | Regex patterns for left foot bone | foot_l, l_foot, ... |
| Right Foot Patterns | Regex patterns for right foot bone | foot_r, r_foot, ... |

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
    │   ├── LocomotionAnimClassifier.h       # Animation classifier
    │   └── BlendSpaceFactory.h              # BlendSpace creator & analyzer
    └── Private/
        ├── BlendSpaceBuilder.cpp
        ├── BlendSpaceBuilderSettings.cpp
        ├── LocomotionAnimClassifier.cpp
        ├── BlendSpaceFactory.cpp
        └── UI/
            ├── SBlendSpaceConfigDialog.*    # Main dialog
            └── SLocomotionAnimSelector.*    # Animation selector
```

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
