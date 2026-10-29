# Same priority: the scalar joins the container in an implicit array
arr_int = 5;
obj_str = "bar";
arr_null = null;

# Containers still merge, including one opened by a nested key
obj_key nested {
	key = value1;
}
obj_brace {
	key1 = value2;
}

# Lower priority than the existing container: the scalar is dropped
arr_lo = 5;
obj_lo = "bar";

# Higher priority than the existing container: the scalar replaces it
.priority 1
arr_hi = 5;
obj_hi = "bar";
