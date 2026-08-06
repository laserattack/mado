#!/bin/bash

COUNT=${1:-10000}
BASE_DIR="MADO"

LONG_TEXT=$(cat << 'EOF'
## Description
This is a detailed description of the entry. It contains multiple paragraphs
to simulate real-world usage of the mado system with substantial content.

## Implementation Details
The implementation follows standard practices with proper error handling
and edge case consideration. All tests pass successfully.

## Notes
- First observation about the implementation
- Second point regarding performance optimization
- Additional consideration for future improvements
- Security review completed and approved
- Documentation has been updated accordingly

## Technical Specifications
The system uses a modular architecture with clear separation of concerns.
Each component is independently testable and follows the single responsibility
principle. The codebase maintains high test coverage and passes all CI checks.

## Review Comments
The code review was conducted on multiple aspects including functionality,
performance, security, and maintainability. All identified issues have been
addressed and verified. The solution demonstrates good engineering practices.

## Performance Metrics
- Response time: < 100ms
- Memory usage: optimized
- CPU utilization: within acceptable limits
- Scalability: tested up to 10x current load

## Dependencies
- Core library v2.1.0 or higher
- Utility package latest stable
- Development tools as specified in documentation

## Changelog
- Added new feature for improved user experience
- Fixed edge case in data processing pipeline
- Updated documentation with examples
- Refactored core module for better maintainability
- Enhanced error messages for debugging

## Configuration
The configuration is straightforward and well-documented. Default values
are sensible and work for most use cases. Advanced options are available
for specific requirements.

## Testing
Comprehensive test suite covers unit tests, integration tests, and
end-to-end scenarios. Regression tests ensure backward compatibility.
Performance benchmarks validate optimization efforts.

## Deployment
Deployment process is automated and includes validation steps. Rollback
procedures are tested and documented. Monitoring alerts are configured
for early detection of issues.

## Support
Documentation is available in the wiki. Team members are available for
questions during business hours. Emergency support is available 24/7
for critical issues.
EOF
)

STEP=$((COUNT / 100))
STEP=$((STEP > 0 ? STEP : 1))

for i in $(seq 1 "$COUNT"); do
    DIR_NAME=$(date -d "2026-01-01 + $((i * 60)) seconds" +%Y%m%dT%H%M%S)

    mkdir -p "$BASE_DIR/$DIR_NAME"

    cat > "$BASE_DIR/$DIR_NAME/MAIN.md" << MDEOF
- NAME: request confirmation during repository initialization
- PRIORITY: 100
- TAGS: feat, ux
- STATUS: closed

$LONG_TEXT

Entry ID: test-$i
Generated: $(date)
MDEOF

    if [ $((i % STEP)) -eq 0 ]; then
        PCT=$((i * 100 / COUNT))
        echo "Progress: $PCT% ($i/$COUNT entries created)..."
    fi
done

echo "Done! Created $COUNT test entries."
