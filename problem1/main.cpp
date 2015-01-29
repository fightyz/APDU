#include <iostream>
#include <map>
#include <vector>
#include <stdlib.h>
#include <string.h>

using namespace std;

bool validate_flag = true;	//用于验证10叉树是否合格

typedef int Element;	//节点元素值
typedef int Position;	//节点按顺序存储编号在10叉树中的位置

//10叉树节点，用节点的指针数组保存10个孩子节点
typedef struct TenTreeNode
{
	Element e;
	Position p;
	TenTreeNode * children[10];
}TenTreeNode;

//一个指向10叉树根节点指针和十叉树节点个数的数据结构
typedef struct TenTree
{
	TenTreeNode *root;
	int count;
}TenTree;

//10叉树节点信息保存在文件中的数据结构
//包括节点元素值和其按顺序存储编号在10叉树中的位置
typedef struct TenTreeNodeFile
{
	Element e;
	Position p;
}TenTreeNodeFile;

//保存 位置 - 节点 信息的map
typedef map<int, TenTreeNode *> PositionNodeMap;

/*
 *	从文件filename中读出10叉树的节点信息，并构建这颗10叉树，返回指向
 *	10叉树的指针
 *
 */
TenTree * readTenTree(char const *filename) {
	TenTree *p_tree = (TenTree *)malloc(sizeof(TenTree));
	TenTreeNodeFile node_file;
	PositionNodeMap position_node_map;
	TenTreeNode *p_node;
	FILE *fp = fopen(filename, "r");

	if(!fp) {
		cerr << "open file error: " << filename << endl;
		exit(EXIT_FAILURE);
	}
	// 首先读出总的节点数
	fread(&(p_tree->count), sizeof(p_tree->count), 1, fp);

	// 读出每一个节点的信息，并保存在 位置 - 节点 map 中，方便下面查找一个节点的子节点
	while(fread(&node_file, sizeof(node_file), 1, fp) > 0) {
		p_node = (TenTreeNode *)malloc(sizeof(TenTreeNode));
		for(int i = 0; i < 10; i++) {
			*(p_node->children + i) = NULL;
		}
		p_node->e = node_file.e;
		position_node_map.insert(pair<int, TenTreeNode *>(node_file.p, p_node));
	}

	//根据 position_node_map 构造10叉树
	PositionNodeMap::iterator iter;
	PositionNodeMap::iterator iter_child;
	for(iter = position_node_map.begin(); iter != position_node_map.end(); iter++) {
		for(int i = 0; i < 10; i++) {
			//子节点的位置编号是父节点位置编号 * 10 - 8 + i, i 表示左数第 i 个孩子
			iter_child = position_node_map.find(iter->first * 10 - 8 + i);
			if(iter_child != position_node_map.end()) {
				*(iter->second->children + i) = iter_child->second;
				//printf("%d %d %d\n", iter->second->e, i, iter_child->second->e);
			} else {
				*(iter->second->children + i) = NULL;
			}
		}
	}
	iter_child = position_node_map.find(1);
	p_tree->root = iter_child->second;
	fclose(fp);
	return p_tree;
}

/*
 *	将10叉树的每个节点都写入到文件中，包括节点元素值和节点位置信息
 *
 */
void writeNode(FILE *p_stream, TenTreeNode *root) {
	fwrite(root, sizeof(Element) + sizeof(Position), 1, p_stream);
	for(int i = 0; i < 10; i++) {
		if(*(root->children + i)) {
			writeNode(p_stream, *(root->children + i));
		}
	}
}

/*
 *	将p_tree所指向的10叉树写入到filename所指向的文件中
 *
 */
void writeTenTree(char const *filename, TenTree *p_tree) {
	FILE *p_stream = fopen(filename, "w+");

	if(!p_stream) {
		cerr << "open file error: " << filename << endl;
		exit(EXIT_FAILURE);
	}

	//先写入10叉树总共的节点数
	fwrite(&p_tree->count, sizeof(int), 1, p_stream);

	//写入每个节点的信息，包括节点元素值和其所在位置
	writeNode(p_stream, p_tree->root);

	fclose(p_stream);
}

int genRandom() {
	return rand() % 10;
}

/*
 *	递归随机生成一颗深度是7，叶子节点高度差不超过1的10叉树
 *
 */
void buildSubTenTree(TenTreeNode *root, int level, int &count) {
	int max_level = 7;
	// 控制叶子结点高度，使所有叶子结点高度都是6或7
	if(level == 6) {
		max_level = rand() % 2 + 6;
	}
	if(level < max_level) {
		int random = genRandom();
		for(int j = 0; j <= random; j++) {
			TenTreeNode *node = (TenTreeNode *)malloc(sizeof(TenTreeNode));
			for(int i = 0; i < 10; i++) {
				*(node->children + i) = NULL;
			}
			node->p = root->p * 10 - 8 + j;
			count++;
			node->e = count;
			*(root->children + j) = node;
			buildSubTenTree(node, level + 1, count);
		}
	} else {
		return;
	}
}

/*
 *	随机生成一颗10叉树
 *
 */
TenTree * buildTenTree() {
	TenTree *p_tree = (TenTree *)malloc(sizeof(TenTree));
	TenTreeNode *root = (TenTreeNode *)malloc(sizeof(TenTreeNode));
	for(int i = 0; i < 10; i++) {
		*(root->children + i) = NULL;
	}
	root->e = 1;
	root->p = 1;
	p_tree->count = 1;
	buildSubTenTree(root, 1, p_tree->count);
	p_tree->root = root;
	return p_tree;
}

/*
 *	释放动态分配的节点空间
 *
 */
void freeSubTenTree(TenTreeNode *root)
{
	for(int i = 0; i < 10; i++) {
		if(*(root->children + i)) {
			freeSubTenTree(*(root->children + i));
		}
	}
	free(root);
}

/*
 *	释放动态分配的节点空间
 *
 */
void freeTenTree(TenTree * p_tree) {
	TenTreeNode *root = p_tree->root;
	free(p_tree);
	freeSubTenTree(root);
}

/*
 *	递归验证每一个节点是否是叶子结点，如果是则判断其高度
 *
 */
void validateSubTree(TenTreeNode *root, int level) {
	bool is_leaf = true;
	for(int i = 0; i < 10; i++) {
		if(*(root->children + i)) {
			is_leaf = false;
			validateSubTree(*(root->children + i), level + 1);
		}
	}
	if(is_leaf) {
		if(level != 6 && level != 7) {
			validate_flag = false;
		}
	}
}

/*
 *	验证这颗10叉树是否满足高度是7，并且叶子结点高度差不超过1
 *
 */
void validate(TenTree *p_tree) {
	TenTreeNode *root = p_tree->root;
	validateSubTree(root, 1);
	if(validate_flag) {
		cout << "This is a Ten Tree." << endl;
	} else {
		cout << "This is not a Ten Tree." << endl;
	}
}

int main() {
	//生成随机数种子，用于之后随机生成10叉树
	srand((unsigned)time(NULL));

	//随机生成一颗10叉树
	TenTree *p_tree = buildTenTree();

	cout << "Generate " << p_tree->count << " nodes." << endl;

	//将10叉树的信息写入到文件中
	writeTenTree("tree.txt", p_tree);

	//free动态分配的10叉树节点
	freeTenTree(p_tree);

	//从文件中读取节点信息，并构造10叉树
	p_tree = readTenTree("tree.txt");

	//验证这颗10叉树是否满足高度是7，并且叶子结点高度差不超过1
	validate(p_tree);

	//free动态分配的10叉树节点
	freeTenTree(p_tree);

	return 0;
}