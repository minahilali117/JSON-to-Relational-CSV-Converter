#!/bin/bash

# Create tests directory if it doesn't exist
mkdir -p tests

# Test 1: Simple flat object
cat > tests/test1.json << EOF
{ "id": 1, "name": "Ali", "age": 19 }
EOF

# Test 2: Array of scalars
cat > tests/test2.json << EOF
{
  "movie": "Inception",
  "genres": ["Action", "Sci-Fi", "Thriller"]
}
EOF

# Test 3: Array of objects
cat > tests/test3.json << EOF
{
  "orderId": 7,
  "items": [
    {"sku": "X1", "qty": 2},
    {"sku": "Y9", "qty": 1}
  ]
}
EOF

# Test 4: Nested objects + reused shape
cat > tests/test4.json << EOF
{
  "postId": 101,
  "author": {"uid": "u1", "name": "Sara"},
  "comments": [
    {"uid": "u2", "text": "Nice!"},
    {"uid": "u3", "text": "+1"}
  ]
}
EOF

# Test 5: Complex JSON with multiple nested structures
cat > tests/test5.json << EOF
{
  "store": {
    "name": "Super Bookstore",
    "location": {"city": "New York", "state": "NY"},
    "established": 1998,
    "categories": ["Fiction", "Science", "History"],
    "books": [
      {
        "title": "The Great Adventure",
        "author": {"name": "J. Smith", "birthYear": 1980},
        "price": 19.99,
        "tags": ["adventure", "bestseller"]
      },
      {
        "title": "Science Explained",
        "author": {"name": "A. Einstein", "birthYear": 1965},
        "price": 29.99,
        "tags": ["education", "science"]
      }
    ],
    "employees": [
      {"id": "E001", "name": "John", "position": "Manager"},
      {"id": "E002", "name": "Mary", "position": "Clerk"}
    ]
  }
}
EOF

# Create test output directory
mkdir -p test_out

# Run tests
echo "Running tests..."

for i in {1..5}; do
    echo -n "Test $i: "
    
    # Create test output directory
    mkdir -p "test_out/test$i"
    
    # Run json2relcsv on test input
    ./json2relcsv < "tests/test$i.json" --out-dir "test_out/test$i"
    
    if [ $? -eq 0 ]; then
        echo "PASS - CSV files generated in test_out/test$i"
        # List generated CSV files
        ls -l "test_out/test$i"
    else
        echo "FAIL - Error processing JSON"
    fi
    echo ""
done

echo "Tests completed."