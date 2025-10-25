FUNCTION Main_Game_Loop()

    // 1. Initialization (Run Once)
    Initialize_Graphics_Window()
    Initialize_Game_State(Game_State)
    Current_Time = GET_HIGH_RES_TIME_MS() // Get initial timestamp
    
    // Schedule initial tasks (e.g., first food spawn)
    ADD_TASK(Task_Heap, Spawn_Food_Task, 5000, 5000, NULL) // func, delay, interval, context

    // --- Core Game Loop (Runs until QUIT) ---
    WHILE Game_Running IS TRUE:

        // 2. Time Management: Calculate elapsed time and cap lag
        New_Time = GET_HIGH_RES_TIME_MS()
        Elapsed_Time = New_Time - Current_Time
        Current_Time = New_Time
        
        // Prevent the "Spiral of Death" where logic takes too long
        IF Elapsed_Time > MAX_LAG_MS:
            Elapsed_Time = MAX_LAG_MS
        
        Lag_Time_MS = Lag_Time_MS + Elapsed_Time

        // 3. Input Processing (Variable Timestep - Run every frame, non-blocking)
        Process_User_Input(Game_State)
        
        // 4. Fixed-Timestep Logic Update (Game Determinism)
        // Run logic in fixed chunks until the 'lag' is consumed
        WHILE Lag_Time_MS >= FIXED_TIME_STEP_MS:
            
            // A. Scheduler Check (The new role of your heap)
            // Execute all events scheduled to run up to the current logic time
            WHILE Task_Heap.PEEK() IS READY AT Current_Time:
                Next_Task = Task_Heap.POP()
                Execute_Task_Function(Next_Task, Game_State) // Tasks modify Game_State
                RESCHEDULE_TASK(Task_Heap, Next_Task)
            
            // B. Core Game Logic (The 'Tick')
            // These functions advance the game state by the fixed delta time.
            Move_Caterpillar(Game_State)        // Move based on input/velocity
            Check_Food_Eaten(Game_State)       // Check for item pickup
            Check_Crash_Detection(Game_State)  // Head vs Tail/Wall
            Update_Score_And_Level(Game_State) // Increment score, check for level up
            
            // Check for game termination after state update
            IF Game_State.Crashed IS TRUE:
                Game_Running = FALSE
                BREAK // Exit inner while loop
                
            // Deduct the time step from the accumulated lag
            Lag_Time_MS = Lag_Time_MS - FIXED_TIME_STEP_MS
            
        END WHILE // Lag consumption loop

        // 5. Rendering (Variable Timestep - Runs as fast as possible)
        // Use a calculated interpolation factor (0.0 to 1.0) to smooth visuals between logic ticks
        Interpolation_Alpha = Lag_Time_MS / FIXED_TIME_STEP_MS
        
        // Render the current state, potentially interpolating between the last
        // fixed state and the predicted current state (optional, but highly recommended for smooth look)
        Render_Game_Screen(Game_State, Interpolation_Alpha)
        
        // 6. Yield/Throttle (Optional: Prevents 100% CPU usage)
        // In C on POSIX systems, you might use a short sleep or yield to save power
        // The time to sleep would be calculated based on the difference between 
        // Current_Time and the next scheduled event time (from Task_Heap.PEEK()),
        // or a small fixed delay (e.g., 4ms) to cap the framerate.
        
    END WHILE // Core Game Loop

    // 7. Cleanup
    Show_Game_Over_Screen(Game_State)
    Cleanup_Task_Heap(Task_Heap)
    Close_Graphics_Window()

END FUNCTION

