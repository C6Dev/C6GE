#pragma once

#include "Object.h"
#include <iostream>

namespace C6GE {

// Example usage of the ECS Object system
class ObjectExample {
public:
    static void DemonstrateUsage(ObjectManager& objectManager) {
        std::cout << "=== ECS Object System Example ===" << std::endl;
        
        // Create a new object
        Object myObject = objectManager.CreateObject("MyObject");
        std::cout << "Created object: " << myObject.GetName() << std::endl;
        
        // Add components
        myObject.AddComponent<Transform>();
        auto& transform = *myObject.GetComponent<Transform>();
        transform.setPosition(1.0f, 2.0f, 3.0f);
        transform.setRotation(0.0f, 45.0f, 0.0f);
        transform.setScale(2.0f);
        
        std::cout << "Added Transform component with position: (" 
                  << transform.position.x << ", " 
                  << transform.position.y << ", " 
                  << transform.position.z << ")" << std::endl;
        
        // Check if object exists
        if (objectManager.ObjectExists("MyObject")) {
            std::cout << "Object 'MyObject' exists!" << std::endl;
        }
        
        // Get the object back
        Object retrievedObject = objectManager.GetObject("MyObject");
        if (retrievedObject.HasComponent<Transform>()) {
            auto& retrievedTransform = *retrievedObject.GetComponent<Transform>();
            std::cout << "Retrieved object has transform at position: (" 
                      << retrievedTransform.position.x << ", " 
                      << retrievedTransform.position.y << ", " 
                      << retrievedTransform.position.z << ")" << std::endl;
        }
        
        // Create another object with instancing
        Object instancedObject = objectManager.CreateObject("InstancedObject");
        instancedObject.AddComponent<Transform>();
        
        // Set up instancing
        Instanced instanced(25); // 5x5 grid
        for (int i = 0; i < 25; ++i) {
            int row = i / 5;
            int col = i % 5;
            float x = (float)(col - 2) * 2.0f;
            float z = (float)(row - 2) * 2.0f;
            instanced.instances[i].setPosition(x, 0.0f, z);
            instanced.instances[i].setScale(0.5f + (float)(i % 3) * 0.2f);
        }
        instancedObject.AddComponent<Instanced>(instanced);
        
        std::cout << "Created instanced object with 25 instances" << std::endl;
        
        std::cout << "=== Example Complete ===" << std::endl;
    }
};

} // namespace C6GE
