cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME GameNetwork)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Windows definitions
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

# Source files
set(SOURCE_FILES
    # Core
    Core/GameNetworkServer.cpp
    Core/GameNetworkClient.cpp
    Core/InterServerCommunication.cpp
    
    # Session
    Session/GameConnection.cpp
    Session/GameSession.cpp
    Session/GameSessionManager.cpp
    Session/Character.cpp
    Session/SessionPersistence.cpp
    Session/ChannelManager.cpp
    Session/DisconnectionHandler.cpp
    
    # Packet
    Packet/GamePacket.cpp
    Packet/PacketProcessor.cpp
    Packet/MessageDispatcher.cpp
    
    # Synchronization
    Synchronization/WorldSynchronizer.cpp
    Synchronization/EntityReplicator.cpp
    Synchronization/EventBroadcaster.cpp
    Synchronization/RegionManager.cpp
    
    # Optimization
    Optimization/NetworkOptimizer.cpp
    Optimization/LatencyCompensator.cpp
    
    # Security
    Security/GamePacketValidator.cpp
    Security/RateLimiter.cpp
    
    # Metrics
    Metrics/NetworkMetrics.cpp
    
    # Replication
    Replication/ReplicationSystem.cpp
    
    # Reliability
    Reliability/ReliableMessageQueue.cpp
    
    # LoadBalancing
    LoadBalancing/LoadBalancer.cpp
    LoadBalancing/ServerMonitor.cpp
    LoadBalancing/PredictiveLoadBalancer.cpp
    LoadBalancing/GeographicLoadBalancer.cpp
    LoadBalancing/AutoScaler.cpp
    LoadBalancing/SeamlessMigration.cpp
    LoadBalancing/HighAvailability.cpp
    
    # Protocol
    Protocol/ReliableUDP.cpp
    Protocol/HybridProtocol.cpp
    
    # Prediction
    Prediction/NetworkPrediction.cpp
    Prediction/LagCompensation.cpp
    
    # Compression
    Compression/DeltaCompression.cpp
    Compression/BitPacker.cpp
    
    # Monitoring
    Monitoring/SystemMonitor.cpp
)

# Header files
set(HEADER_FILES
    # Constants
    GameNetworkConstants.h
    
    # Core
    Core/GameNetworkServer.h
    Core/GameNetworkClient.h
    Core/InterServerCommunication.h
    
    # Session
    Session/GameConnection.h
    Session/GameSession.h
    Session/GameSessionManager.h
    Session/Character.h
    Session/SessionPersistence.h
    Session/ChannelManager.h
    Session/DisconnectionHandler.h
    
    # Packet
    Packet/GamePacket.h
    Packet/PacketProcessor.h
    Packet/MessageDispatcher.h
    
    # Synchronization
    Synchronization/WorldSynchronizer.h
    Synchronization/EntityReplicator.h
    Synchronization/EventBroadcaster.h
    Synchronization/RegionManager.h
    
    # Optimization
    Optimization/NetworkOptimizer.h
    Optimization/LatencyCompensator.h
    
    # Security
    Security/GamePacketValidator.h
    Security/RateLimiter.h
    
    # Metrics
    Metrics/NetworkMetrics.h
    
    # Replication
    Replication/ReplicationSystem.h
    
    # Reliability
    Reliability/ReliableMessageQueue.h
    
    # LoadBalancing
    LoadBalancing/LoadBalancer.h
    LoadBalancing/ServerMonitor.h
    LoadBalancing/PredictiveLoadBalancer.h
    LoadBalancing/GeographicLoadBalancer.h
    LoadBalancing/AutoScaler.h
    LoadBalancing/SeamlessMigration.h
    LoadBalancing/HighAvailability.h
    
    # Protocol
    Protocol/ReliableUDP.h
    Protocol/HybridProtocol.h
    
    # Prediction
    Prediction/NetworkPrediction.h
    Prediction/LagCompensation.h
    
    # Compression
    Compression/DeltaCompression.h
    Compression/BitPacker.h
    
    # Monitoring
    Monitoring/SystemMonitor.h
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${LIBRARY_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    Core 
    Session 
    Packet 
    Synchronization 
    Optimization 
    Security 
    Metrics 
    Replication 
    Reliability 
    LoadBalancing
    Protocol
    Prediction
    Compression
    Monitoring
)

# Find packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(glm CONFIG REQUIRED)

# Link libraries
target_link_libraries(${LIBRARY_NAME} 
    PUBLIC 
        Network          # Base network module
        Thread           # Thread pool
        Utilities        # Utilities
        Redis            # For session persistence
        Boost::system
        glm::glm
)

# Optional compression support
option(USE_COMPRESSION "Enable packet compression" ON)
if(USE_COMPRESSION)
    find_package(ZLIB REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC ZLIB::ZLIB)
    target_compile_definitions(${LIBRARY_NAME} PUBLIC USE_COMPRESSION)
endif()

# Optional encryption support
option(USE_ENCRYPTION "Enable packet encryption" ON)
if(USE_ENCRYPTION)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(${LIBRARY_NAME} PUBLIC USE_ENCRYPTION)
endif()
