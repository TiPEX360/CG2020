#include "KDTree.h"

Node* KDTree::insert(Node *root, glm::vec4 value, int depth) {
    int axis = depth % 3;
    if(root == NULL) {
        // std::cout << "newnode" <<std::endl;
        Node* n = new Node{glm::vec3(value[0], value[1], value[2]), value[3], NULL, NULL};
        return n;
    }
    if(value[axis] < root->loc[axis]) {
        // std::cout << "l";
        root->left = insert(root->left, value, depth + 1);
    }else {
        // std::cout << "r";
        root->right = insert(root->right, value, depth + 1);
    }
    return root;
}

Node* KDTree::nearestSearch(Node *root, glm::vec3 key, int depth, Node* nearest, float minDistance) {
    int axis = depth % 3;
    if(root == NULL) {
        return NULL;
    }
    //Add node to list and see if potentiallty nearest
    // std::cout << minDistance << std::endl;  ////
    // for(int i = 0; i < traversed.size(); i++) {
    //     if(glm::distance(root->loc, key) < glm::distance(key, traversed[i]->loc)) {
    //         traversed.insert(traversed.begin()+i, root); break;
    //     }
    // }

    traversed.push_back(root);
    if(glm::distance(root->loc, key) < minDistance || nearest == NULL) {
        nearest = root;
        // std::cout << "1" << std::endl;
        minDistance = glm::distance(root->loc, key);
    }
    bool left;
    if(key[axis] < root->loc[axis]) {
        //go left WITHIN this box
        left = true;
        Node *tempNearest = nearestSearch(root->left, key, depth+1, nearest, minDistance);
        if(tempNearest != NULL) nearest = tempNearest;
        // std::cout << "2" << std::endl;
    }else {
        //go right WITHING this box
        left = false;
        Node *tempNearest = nearestSearch(root->right, key, depth+1, nearest, minDistance);
        if(tempNearest != NULL) nearest = tempNearest;
        
        // std::cout << "3" << std::endl;
    }
    //update distance
    // std::cout << nearest->loc[axis] << std::endl;
    minDistance = glm::distance(nearest->loc, key);
    //distance from OUTER box
    float boxDistance = glm::abs(key[axis] - root->loc[axis]);
    if(boxDistance < minDistance) {
        //go other way
        if(left) {
            Node *tempNearest = nearestSearch(root->right, key, depth+1, nearest, minDistance);
            if(tempNearest != NULL) nearest = tempNearest;
        } else {
            Node *tempNearest = nearestSearch(root->left, key, depth+1, nearest, minDistance);
            if(tempNearest != NULL) nearest = tempNearest;
        }
        minDistance = glm::distance(nearest->loc, key);
    }

    return nearest;
}

std::vector<Node*> KDTree::beginSearch(glm::vec3 loc) {
    traversed.clear();
    Node* nearest = nearestSearch(root, loc, 0, NULL, INFINITY);
   
    traversed.push_back(nearest);
        // std::cout << "4" << std::endl;
    return traversed;
}

KDTree::KDTree(glm::vec4 value) {
    root = new Node{glm::vec3(value[0], value[1], value[2]), value[3], NULL, NULL};
}
KDTree::KDTree() {
    root = NULL;
}