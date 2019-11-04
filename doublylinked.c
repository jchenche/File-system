#include <stdio.h>
#include <stdlib.h>

typedef struct Node Node;
struct Node {
	char* command;
	Node* prev;
	Node* next;
};

typedef struct List List;
struct List {
	Node* head;
	Node* tail;
};

List* make_list() {
	List* list = (List*) malloc(sizeof(List));
	if (list == NULL) {
		fprintf(stderr, "Error allocating memory for list.\n");
		return NULL;
	}
	list->head = NULL;
	list->tail = NULL;
	return list;
}

Node** list_head_p(List* list) {
	return &(list->head);
}

Node** list_tail_p(List* list) {
	return &(list->tail);
}

Node* make_node(char* command) {
	Node* new_node = (Node*) malloc(sizeof(Node));
	if (new_node == NULL) {
		fprintf(stderr, "Error allocating memory for node.\n");
		return NULL;
	}
	new_node->command = command;
	new_node->prev = NULL;
	new_node->next = NULL;
	return new_node;
}

Node* find_data(Node* head, char* elem) {
	Node* ref = head;
    while(ref != NULL && ref->command != elem) {
        ref = ref->next;
    }
    return ref;
}

void swap(Node* ref1, Node* ref2) {

    /*
     * Nodes could be far from each other
     * Nodes could be next to each other ref2 after ref1
     * Nodes could be next to each other ref1 after ref2
     */
    Node* ref1_p = ref1->prev;
    Node* ref1_n = ref1->next;
    Node* ref2_p = ref2->prev;
    Node* ref2_n = ref2->next;

    if (ref1_n == ref2) { /* If ref2 is immediate to the right of ref1 */
        ref1_n = ref1;
        ref2_p = ref2;
    } else if (ref1_p == ref2) { /* If ref2 is immediate to the left of ref1 */
        ref2_n = ref2;
        ref1_p = ref1;
    }

    if(ref1_p) ref1_p->next = ref2; /* If ref1 is a head, then ref1_p == NULL */
    ref2->prev = ref1_p;
    if(ref1_n) ref1_n->prev = ref2; /* If ref1 is a tail, then ref1_n == NULL */
    ref2->next = ref1_n;
    
    if(ref2_p) ref2_p->next = ref1; /* If ref2 is a head, then ref2_p == NULL */
    ref1->prev = ref2_p;
    if(ref2_n) ref2_n->prev = ref1; /* If ref2 is a tail, then ref2_n == NULL */
    ref1->next = ref2_n;
}

void insert(Node** head, Node** tail, char* command) {
	Node* new_node = make_node(command);
	if (*head == NULL) {
		*head = new_node;
		*tail = new_node;
	} else {
		(*tail)->next = new_node;
		new_node->prev = *tail;
		*tail = new_node;
	}
}

void insert_front(Node** head, Node** tail, char* command) {
	Node* new_node = make_node(command);
	if (*tail == NULL) {
		*head = new_node;
		*tail = new_node;
	} else {
		(*head)->prev = new_node;
		new_node->next = *head;
		*head = new_node;
	}
}

void insert_all(Node** head, Node** tail, char** commands, int length) {
	for (int i = 0; i < length; i++) insert(head, tail, commands[i]);
}

void pop_back(Node** head, Node** tail) {
	if (head != NULL) { /* Make sure it's not empty */
		Node* temp = *tail;
		if ((*tail)->prev == NULL) { /* Contains one node */
			*head = NULL;
			*tail = NULL;
		} else {
			*tail = (*tail)->prev;
			(*tail)->next = NULL;
		}
		free(temp);
	}
}

void pop_front(Node** head, Node** tail) {
	if (head != NULL) { /* Make sure it's not empty */
		Node* temp = *head;
		if ((*head)->next == NULL) { /* Contains one node */
			*head = NULL;
			*tail = NULL;
		} else {
			*head = (*head)->next;
			(*head)->prev = NULL;
		}
		free(temp);
	}
}

void print_list(Node* head) {
	for(; head != NULL; head = head->next) printf("%s ", head->command);
	printf("\n");
}

void print_list_backward(Node* tail) {
	for (; tail != NULL; tail = tail->prev) printf("%s ", tail->command);
	printf("\n");
}

int main() {
	List* dl = make_list();
	insert(list_head_p(dl), list_tail_p(dl), "3");
	pop_front(list_head_p(dl), list_tail_p(dl));
	char* commands[] = {"1","3","5","7","nine", "hello"};
	insert_all(list_head_p(dl), list_tail_p(dl), commands, sizeof(commands)/sizeof(char*));
	insert_front(list_head_p(dl), list_tail_p(dl), "10");
	pop_front(list_head_p(dl), list_tail_p(dl));
	pop_back(list_head_p(dl), list_tail_p(dl));
	print_list(dl->head);
	print_list_backward(dl->tail);
	return 0;
}
