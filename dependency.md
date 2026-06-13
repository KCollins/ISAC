```mermaid
---
title: ISAC Dependency Graph
---

flowchart LR
    %% Sensors grouped at the bottom, pointing up to the Drogue
    subgraph Sensors [Environmental Sensors]
        G(Optical Backscatter Sensor)
        H(Chlorophyll A Sensor)
    end

    %% Core Components
    subgraph Structural [Structural Components]
        A(Column Drogue)
        B(Float)
    end

    subgraph Electronic [Electronic Subsystems]
        C(Power Bus)
        D(Telemetry System)
    end

    subgraph Electromechanical [Electromechanical Subsystems]
        E(Automated Water Sampling System)
        F(Automated Ash Dispersal System)
    end

    %% System Hierarchy and Flow
    %% B-->A
    E -->A
    E --> C
    E --> D
    E --> B

    F --> C
    F --> D

    D --> C
    D --> B
    C --> B

    %% Connecting Sensors to Drogue
    H --> A
    G --> A

    %% %% Invisible structural links to keep layout clean and prevent overlap
    B ~~~ A
    G ~~~ Sensors
    style Structural fill:#dcfce7,stroke:#16a34a,stroke-width:2px
    style Electromechanical fill:#fee2e2,stroke:#dc2626,stroke-width:2px
    style Electronic fill:#ADD8E6,stroke:#0116ff,stroke-width:2px
    %% style Sensors fill:#FFFFED,stroke:#dc2676,stroke-width:2px
```

```mermaid
---
title: ISAC Dependency Graph
---
flowchart LR
    %% Subgraphs
    subgraph Electromechanical [Electromechanical Subsystems]
        E(Automated Water <br>Sampling System)
        F(Automated Ash <br>Dispersal System)
    end

    subgraph Electronic [Electronic Subsystems]
        D(Telemetry System)
        C(Power Bus)
    end

    subgraph Structural [Structural Components]
        B(Float)
        A(Column Drogue)
    end

    subgraph Sensors [Environmental Sensors]
        G(Optical <br>Backscatter)
        H(Chlorophyll A)
        I(Optical and Acoustic Sensors)
    end

    %% --- Streamlined Connections ---
    
    %% Electromechanical Layer
    E --> A
    E --> B
    E --> C
    E --> D

    F --> C
    F --> D

    %% Electronic Layer
    D --> C
    D --> B
    C --> B

    %% Sensors Layer
    G --> A
    H --> A
    I-->A

    %% Invisible vertical constraints to stack nodes vertically inside subgraphs
    E ~~~ F
    D ~~~ C
    A ~~~ B
    G ~~~ H

    %% Style Definitions
    style Structural fill:#dcfce7,stroke:#16a34a,stroke-width:2px
    style Electromechanical fill:#fee2e2,stroke:#dc2626,stroke-width:2px
    style Electronic fill:#ADD8E6,stroke:#0116ff,stroke-width:2px
    style Sensors fill:#fffde6,stroke:#FFDB58,stroke-width:2px
```
