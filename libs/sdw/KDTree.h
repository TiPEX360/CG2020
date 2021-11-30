#pragma once

#include "glm/glm.hpp"
#include <vector>
#include <iostream>

struct Node {
    glm::vec3 loc;
    float intensity;
    Node* left;
    Node* right;
};

class KDTree {
public:
    Node *root;

    std::vector<Node*> beginSearch(glm::vec3 loc);

    Node *insert(Node *root, glm::vec4 value, int depth);
    Node *nearestSearch(Node *root, glm::vec3 key, int depth, Node* nearest, float distance);
    KDTree(glm::vec4 value);
    KDTree();
private:
    std::vector<Node*> traversed; //clear this

};