
void example_dynamic_rbtree(void)
{
	// create a dynamic tree, push 2 nodes on to it, find and delete them.
	
	// create a new tree
	struct edit_tree fred;
	RB_INIT(&fred);
	
	struct FByte *new_node;		// this will be malloc'd for each new node
	struct FByte s, *f; 		// s for search criteria, *f pointer to found node
	char proof[3] = "xy"; 		// update xy to ab and prove it worked
	
	//push 7,a
	new_node = xmalloc(sizeof(*new_node));
	new_node->offset = 7;
	// DEMO ONLY. This next line BAD programming, but functionally equivalent of -> by 
	// dereferencing new_node and then using the . operator. Brackets are required as
	// . has a higher operator order than *, so otherwise it would not compile
	(*new_node).byte = 'a';  
	RB_INSERT(edit_tree,&fred,new_node);
	
	// push 4,b
	new_node = xmalloc(sizeof(*new_node));
	new_node->offset = 4;
	new_node->byte = 'b';
	RB_INSERT(edit_tree,&fred,new_node);
	
	// find them and delete them
	s.offset = 7;
	f = RB_FIND(edit_tree, &fred, &s);
	if (f) {
		proof[0] = f->byte; 
		RB_REMOVE(edit_tree, &fred, f);
		free(f);
	}
	
	s.offset = 4;
	f = RB_FIND(edit_tree, &fred, &s);
	if (f) {
		proof[1] = f->byte; 
		RB_REMOVE(edit_tree, &fred, f);
		free(f);
	}
	
	popup_question(proof, "", PTYPE_CONTINUE);

}
